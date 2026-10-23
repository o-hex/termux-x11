# Termux-X11 Architecture, Fixes & Hardware Integration Index

> [!IMPORTANT]
> **Context Restoration Invariant:**
> This document is the definitive technical record of all bug fixes, architectural patches, hardware alignment invariants, and configuration states applied to Termux-X11 and the desktop environment on this device. When resuming in a fresh session with pruned context, read this file first.

---

## 1. Target Hardware & Platform Specification

* **Device / Platform:** Qualcomm Snapdragon 8 Elite (SM8750 / `piano_global` / Android 15 & 16 / Linux aarch64).
* **GPU / Graphics Core:** Adreno (TM) 830v1.
* **Kernel & Driver Interface:** Direct KGSL driver (`/dev/kgsl-3d0`) with hardware `dma_heap` and zero-copy `AHardwareBuffer` allocation.
* **X11 Acceleration Stack:** `termux-x11` standalone X server (using Android `app_process64`) + Mesa Gallium / Freedreno DRI3 zero-copy pipeline + `picom v13` compositor (`backend = "glx"`, `vsync = true`, `dual_kawase` blur).
* **Repositories:**
  * Local Repo: `/data/data/com.termux/files/home/Projects/termux_x11_upscaler/termux-x11`
  * Fork Remote: `https://github.com/o-hex/termux-x11.git` (branches: `custom-build`, `master`)
  * Upstream: `https://github.com/termux/termux-x11.git`
  * Deliverable Path: `/sdcard/Download/termux-x11-universal-debug.apk`

---

## 2. Exhaustive Index of Applied Fixes

### Fix 1: Adreno 830 Macro-Tiling 4-Line Height Alignment (GLX Black Windows)
* **Problem:** Under `picom --backend glx` (or any OpenGL/KWin compositor), all client windows rendered as pitch-black rectangles.
* **Root Cause:** Adreno 830 hardware macro-tiling requires all texture heights sampled via `GLX_EXT_texture_from_pixmap` / `EGL_NATIVE_PIXMAP_KHR` to align to 4-line boundaries. When an app requested a height not divisible by 4 (e.g. `850x650` where `650 % 4 == 2`), Freedreno/Turnip threw `GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT` (`0x8cd6`), forcing all texture samples to transparent black `(0, 0, 0, 0)`.
* **Fix Implemented:**
  1. In `lorie/src/main/cpp/lorie/buffer.c`, `LorieBuffer_createRegion()` aligns the backing buffer allocation height:
     ```c
     uint16_t aligned_height = (height + 3) & ~3;
     ```
  2. In `lorie/src/main/cpp/patches/xserver.patch`, patched `proc_dri3_buffer_from_pixmap()` to report the aligned stride/height for DRI3 export:
     ```c
     lines = (pPixmap->drawable.height + 3) & ~3;
     ```
  3. Window geometries, viewports, mouse clicks, and input coordinates remain exact (e.g., 650px); only the GPU's internal memory buffer stride is padded with invisible lines.
* **Commits:** [`b50e7dc`](https://github.com/o-hex/termux-x11/commit/b50e7dc0375281bfcdc0e763d44ae6d477cffda9), [`37cd136`](https://github.com/o-hex/termux-x11/commit/37cd136).

---

### Fix 2: Non-Blocking IPC Socket (`O_NONBLOCK`) to Prevent Companion App Deadlocks
* **Problem:** Backgrounding or minimizing the Termux-X11 companion app froze the entire X server (`Thread-2` deadlock).
* **Root Cause:** In `lorie/src/main/cpp/lorie/cmdentrypoint.cpp`, the communication channel between the Android UI thread and the X server thread used a blocking `socketpair()`. When Android throttled background processes, blocking writes wedged the X server render loop.
* **Fix Implemented:** Set `O_NONBLOCK | O_CLOEXEC` on the socketpair in `cmdentrypoint.cpp`.
* **Commit:** [`b50e7dc`](https://github.com/o-hex/termux-x11/commit/b50e7dc0375281bfcdc0e763d44ae6d477cffda9).

---

### Fix 3: Reconnect Broadcast Storm & 250ms Polling Loop Elimination
* **Problem:** Termux-X11 spun a continuous 250ms polling loop in the background, knocking on the socket indefinitely, flooding Android's `system_server` with broadcast intents, and draining battery.
* **Root Cause:**
  1. In `MainActivity.java`, `scheduleConnect()` unconditionally queued `handler.postDelayed(connectRetry, 250)`. Crucially, upstream never unregistered `connectRetry` in `onPause()`.
  2. In `CmdEntryPoint.java`, every socket knock immediately fired an unthrottled `sendBroadcast(intent)`.
* **Fix Implemented:**
  1. Replaced static 250ms loop with progressive exponential backoff (250ms → 500ms → 1000ms max) in `MainActivity.java`.
  2. Added lifecycle unregistration: `handler.removeCallbacks(connectRetry)` inside `onPause()`, `clientConnectedStateChanged()`, and `tryConnect()`.
  3. Added debouncing synchronization flag `broadcastPending` in `CmdEntryPoint.java` that coalesces rapid knock bursts into a single broadcast.
* **Commits:** [`0fff278`](https://github.com/o-hex/termux-x11/commit/0fff278), [`9e81b40`](https://github.com/o-hex/termux-x11/commit/9e81b40).

---

### Fix 4: Native Embedded GLSL Upscaling Engine & CLI Switcher (`set-scaler`)
* **Problem:** Bilinear presentation blur on high-DPI displays. Prior implementations relied on 64-frame polling `stat()` disk checks that failed across Android cross-UID scoped storage boundaries.
* **Fix Implemented:**
  1. Embedded all 6 high-performance GLSL scalers directly into C++ header `lorie/src/main/cpp/lorie/builtin_shaders.h`:
     * AMD FSR 1.0 (EASU edge-adaptive upscaler + RCAS sharpening)
     * Bicubic Catmull-Rom + CAS (BiCAS)
     * Qualcomm Snapdragon Game Super Resolution (SGSR)
     * NVIDIA Image Scaling (NIS)
     * Robust Contrast-Adaptive Sharpening (RCAS)
     * EWA Lanczos-3 Anti-Ringing
  2. Added `rendererScaler` list preference to Android `preferences.xml` and `arrays.xml`.
  3. In `renderer.cpp`, `Renderer::setCustomScaler` instantly compiles and binds the embedded shaders on the GPU thread for both RGBA and BGRA framebuffers with zero file I/O latency.
  4. Added cross-UID fallback on `/sdcard/Download/termux_upscaler.glsl` for custom user-authored shaders.
  5. Uniform bindings: explicitly bind `u_tex_size`, `u_texel_size`, `u_dst_size`, `u_sharpness`.
  6. Provided instant CLI control script `~/bin/set-scaler` (in `PATH`):
     ```bash
     set-scaler fsr       # Activates AMD FSR 1.0
     set-scaler sgsr      # Activates Qualcomm SGSR
     set-scaler bicas     # Activates Bicubic + CAS
     set-scaler off       # Reverts to clean bilinear GLES2
     set-scaler status    # Prints current active scaler preference
     ```
* **Commits:** [`82f45ad`](https://github.com/o-hex/termux-x11/commit/82f45ad), [`ab5674c`](https://github.com/o-hex/termux-x11/commit/ab5674c), [`584584a`](https://github.com/o-hex/termux-x11/commit/584584a).

---

### Fix 5: Present GPU Offload Coherence Probe Fix (uDroid Patch `0023`)
* **Problem:** In logcat, Termux-X11 reported:
  `0/243 present copies offloaded to GPU (CPU fallback: 0 too many rects, 0 queue full, 243 other)`
  100% of window frames were falling back to slow CPU memory copying (`memcpy` / `pixman_blt`), wasting 3+ GB/s of RAM bandwidth.
* **Root Cause:** In `renderer.cpp`, `cpuAndGpuShareAhbMemory()` tested CPU/GPU coherency by acquiring a CPU lock via `AHardwareBuffer_lock()` and issuing `glClear()` **while the buffer was still locked by the CPU**. Under Android Gralloc specifications, concurrent CPU/GPU access is illegal. On Adreno drivers, cache invalidation only triggers upon `AHardwareBuffer_unlock()`. Because the buffer stayed locked, the test read zeros, failed, and permanently set `gpuPresentDisabled = TRUE`.
* **Fix Implemented:**
  1. In `renderer.cpp`: Drop CPU lock before `glClear`, issue `glFinish()`, then re-lock with `AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN` to verify the written pixel.
  2. In `InitOutput.c` (`lorieTryScheduleGpuCopy`): Yield idle destination CPU mappings back to the GPU before enqueuing GPU Present copies (`LorieBuffer_unlock(dstPriv->buffer)`).
* **Commit:** [`c630fa1`](https://github.com/o-hex/termux-x11/commit/c630fa1).

---

### Fix 6: Preserving Logical Pixmap Dimensions for Padded AHBs (uDroid Patch `0021`)
* **Problem:** The physical height alignment `(height + 3) & ~3` added 1–3 invisible padding lines to the hardware buffer. In upstream `InitOutput.c`, `ModifyPixmapHeader` previously passed physical `desc.width, desc.height`, which erroneously enlarged the logical X11 pixmap geometry.
* **Fix Implemented:** In `loriePixmapFromFds()`, pass logical `width, height` to `screen->ModifyPixmapHeader(pixmap, width, height, 0, 0, desc.stride * 4, NULL)`. Added validation assertion `check(desc.width < width || desc.height < height, ...)`.
* **Commit:** [`9ffb308`](https://github.com/o-hex/termux-x11/commit/9ffb308).

---

### Fix 7: Bounded DRI3 Socket Handshake Timeout & Modifier 1257 (uDroid Patch `0007`)
* **Problem:** If a 3D client crashed while negotiating DRI3 buffer sharing, the X server hung indefinitely on blocking `write()` or `recvHandle()`.
* **Fix Implemented:** Added `lorieWaitForSocketEvent()` with a 3,000 ms timeout (`AHB_HANDSHAKE_TIMEOUT_MS`) using `poll()` for socket readiness. Added support for `AHARDWAREBUFFER_RGBA_SOCKET_FD` (1257).
* **Commit:** [`9ffb308`](https://github.com/o-hex/termux-x11/commit/9ffb308).

---

### Fix 8: GTK4 Client-Side Decoration (CSD) Blur Halo Elimination
* **Problem:** Windows running under GTK4 (e.g. `ghostty`) exhibited an oversized 25px fuzzy blurred border that overflowed past the window frame.
* **Root Cause:** GTK4 CSD allocates a transparent 25px drop-shadow boundary reported via `_NET_FRAME_EXTENTS = 25, 25, 25, 25`. Picom's dual-kawase blur shader blurred this entire reserved frame extent.
* **Fix Implemented:** Added `window-decoration = none` to `~/.config/ghostty/config`. Verified with `xprop` that frame extents are strictly `0, 0, 0, 0`.

---

### Fix 9: Authentic Desktop Backup Restoration
* **Source:** Restored from the authentic backup tree in `~/.config/desktop_backup_20261010_224305/`.
* **Parameters Applied Live:**
  * Panel Height: **`40px`** (`/panels/panel-1/size`)
  * Icon Sizing: **`0` (Automatic)** (`/panels/panel-1/icon-size`)
  * Systray Icon Size: **`22px`** (`/plugins/plugin-6/icon-size`)
  * Clock Font: **`AndroidClock Bold 16`** (`/plugins/plugin-8/digital-time-font`)
  * PulseAudio Volume Icon: `-gtk-icon-transform: scale(0.8);` in `~/.config/gtk-3.0/gtk.css`
  * Theme: `Fluent-compact` | Icons: `YAMIS` / `Colloid-Dark` | Cursors: `breeze_cursors`

---

### Fix 10: Touch Palm Rejection & Multi-Pointer Demuxing
* **Fix Implemented:** Deterministic heuristic palm filter and multi-touch demuxing in `TouchInputHandler.java` (commit `369508a` and `03899b7`).

---

### Fix 11: Upstream Master Integration (Twaik presentation & lock fixes)
* **Root Window Lock Duration (`701ecd5` & `09194f2`)**:
  * `ExaCheckCopyWindow` in `exa/exa_unaccel.c` was preparing the root window as `EXA_PREPARE_SRC` during window moves/scrolls, bypassing `lorieNeedsGpuLock()`. Fixed by changing to `EXA_PREPARE_DEST`.
  * Added `lorieKeepLockUntilRequestEnd()`: Destination locks on the root window are now held across the entire X11 request until `ddxRequestDone()` in `dix/dispatch.c`, eliminating "ghost window" replicas caused by GLES renderer sampling mid-draw.
* **Robust Mutex `connFd` Verification (`7e5f5c5`)**:
  * `lorie_mutex_lock` in `lorie.h` timed out after 33ms and checked `lorieConnectionAlive()`. In the companion app renderer process, `conn_fd` was uninitialized (-1), causing it to believe the X server died and re-initialize the mutex while Xorg was actively writing. Fixed by passing `connFd` explicitly.
* **Linux Kernel Futex Lock Handoff (`9ebfa66`)**:
  * Replaced `sched_yield()` + `usleep(20)` polling loop in `lorieRequestLockRelease()` with `SYS_futex` (`FUTEX_WAIT_BITSET` / `FUTEX_WAKE`), cutting CPU overhead.
* **Process-Local Lock State Leakage (`3960a15`)**:
  * `buffer.locked` and `buffer.lockedData` are reset upon receiving an AHB handle across UNIX sockets, preventing sender pointer leakage into the receiver.

---

## 3. How to Trigger Clean CI Builds

Builds are executed via GitHub Actions on the `o-hex/termux-x11` fork:

```bash
# 1. Inspect status
git -C ~/Projects/termux_x11_upscaler/termux-x11 status

# 2. Push changes to custom-build and master
git -C ~/Projects/termux_x11_upscaler/termux-x11 push fork custom-build
git -C ~/Projects/termux_x11_upscaler/termux-x11 push fork custom-build:master

# 3. Dispatch CI workflow
gh workflow run debug_build.yml --ref master --repo o-hex/termux-x11

# 4. Monitor build progress
gh run list --repo o-hex/termux-x11 -L 3
gh run view <run-id> --repo o-hex/termux-x11

# 5. Download and stage APK to device Downloads
mkdir -p $PREFIX/tmp/apk_dl
gh run download <run-id> --repo o-hex/termux-x11 -n termux-x11-universal-debug -D $PREFIX/tmp/apk_dl
cp $PREFIX/tmp/apk_dl/termux-x11-universal-debug.apk /sdcard/Download/termux-x11-universal-debug.apk
rm -rf $PREFIX/tmp/apk_dl
```

---

## 4. Quick Verification Commands

```bash
# Check running X server & compositor processes
ps -ef | grep -E "termux-x11|picom|ghostty|xfce4" | grep -v grep

# Check GPU dmabufs and KGSL node
ls -l /proc/$(pgrep -f "termux-x11 com.termux.x11")/fd | grep -E "dmabuf|kgsl"

# Verify Present GPU offload counter in live telemetry (must show offloads > 0, not all other)
logcat -d -s LorieNative:V | grep "present copies offloaded" | tail -n 10

# Test scaler switcher
set-scaler fsr
set-scaler status

# Verify scrot capture with no black windows
DISPLAY=:0 scrot /tmp/test.png && python3 -c "from PIL import Image; print(len(set(Image.open('/tmp/test.png').getdata())))" && rm -f /tmp/test.png
```
