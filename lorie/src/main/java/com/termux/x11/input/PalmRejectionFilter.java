package com.termux.x11.input;

import android.os.Build;
import android.os.SystemClock;
import android.view.MotionEvent;

/**
 * Deterministic multi-stage palm rejection engine for Termux:X11.
 * Supports pen-priority temporal hysteresis, contact ellipse / touch major
 * geometry classification, native driver palm flags, and pen-only isolation.
 */
public class PalmRejectionFilter {
    public static final int MODE_DISABLED = 0;
    public static final int MODE_STYLUS_PRIORITY = 1;
    public static final int MODE_STYLUS_AND_AREA = 2;
    public static final int MODE_PEN_ONLY = 3;

    // Native Android MotionEvent constants (API 29+)
    public static final int CLASSIFICATION_AMBIGUOUS_GESTURE = 1;

    private int mMode = MODE_STYLUS_AND_AREA;
    private long mStylusHysteresisMs = 450; // Grace window after pen stroke lift
    private float mPalmTouchMajorThreshold = 110f; // Max major axis length in pixels for finger
    private float mPalmSizeThreshold = 0.30f; // Max normalized size for finger
    private float mPalmEccentricityThreshold = 2.0f; // Major/Minor axis ratio for angled palm contact
    private float mEdgeMarginFraction = 0.03f; // Outer bezel margin (3%)

    private long mLastStylusTimestamp = 0;
    private boolean mStylusIsDown = false;
    private boolean mFingerWasDown = false;
    private boolean mNeedFingerCancel = false;

    public PalmRejectionFilter() {}

    public void setMode(int mode) {
        this.mMode = mode;
    }

    public int getMode() {
        return mMode;
    }

    public void setStylusHysteresisMs(long ms) {
        this.mStylusHysteresisMs = Math.max(0, ms);
    }

    public long getStylusHysteresisMs() {
        return mStylusHysteresisMs;
    }

    public void setPalmTouchMajorThreshold(float thresholdPx) {
        this.mPalmTouchMajorThreshold = Math.max(10f, thresholdPx);
    }

    public void setPalmSizeThreshold(float threshold) {
        this.mPalmSizeThreshold = Math.max(0.01f, threshold);
    }

    /**
     * Updates internal stylus tracking state.
     * Called whenever a stylus or eraser pointer event occurs.
     */
    public void notifyStylusEvent(long nowMs, boolean isDown) {
        this.mLastStylusTimestamp = nowMs;
        this.mStylusIsDown = isDown;
        if (isDown && mFingerWasDown) {
            // Retroactive cancellation: finger touched slightly before pen landing
            mNeedFingerCancel = true;
        }
    }

    /**
     * Checks and resets retroactive cancellation flag.
     */
    public boolean checkAndClearNeedFingerCancel() {
        boolean need = mNeedFingerCancel;
        mNeedFingerCancel = false;
        return need;
    }

    public void notifyFingerTouch(boolean isDown) {
        this.mFingerWasDown = isDown;
    }

    /**
     * Pure deterministic decision core. Evaluates whether a touch pointer should be rejected.
     */
    public boolean shouldRejectPointer(
            long nowMs,
            int toolType,
            int actionMasked,
            float x, float y,
            float touchMajor, float touchMinor,
            float size,
            int classification,
            float viewWidth, float viewHeight) {

        if (mMode == MODE_DISABLED) {
            return false;
        }

        // Stylus and mouse events are never rejected by the palm filter
        if (toolType == MotionEvent.TOOL_TYPE_STYLUS || toolType == MotionEvent.TOOL_TYPE_ERASER
                || toolType == MotionEvent.TOOL_TYPE_MOUSE) {
            return false;
        }

        // Stage 0: Pen-Only mode unconditionally blocks all finger/touch input
        if (mMode == MODE_PEN_ONLY) {
            return true;
        }

        // Stage 1: Native driver palm classification
        if (classification == CLASSIFICATION_AMBIGUOUS_GESTURE) {
            return true;
        }

        // Stage 2: Asymmetric Pen Priority & Temporal Hysteresis Window
        if (mMode == MODE_STYLUS_PRIORITY || mMode == MODE_STYLUS_AND_AREA) {
            if (mStylusIsDown) {
                return true; // Active pen on screen: drop all finger touches
            }
            long elapsedSinceStylus = nowMs - mLastStylusTimestamp;
            if (mLastStylusTimestamp > 0 && elapsedSinceStylus >= 0 && elapsedSinceStylus < mStylusHysteresisMs) {
                return true; // Within air-gap grace window: resting palm between strokes
            }
        }

        // Stage 3: Contact Geometry & Ellipse Area Classifier
        if (mMode == MODE_STYLUS_AND_AREA) {
            // Normalized size threshold
            if (size >= mPalmSizeThreshold) {
                return true;
            }

            // Touch major axis threshold (calibrated for standard display density)
            if (touchMajor >= mPalmTouchMajorThreshold) {
                return true;
            }

            // Eccentricity check: palm heel pressed obliquely has high major-to-minor ratio
            if (touchMinor > 0 && (touchMajor / touchMinor) >= mPalmEccentricityThreshold
                    && touchMajor >= (mPalmTouchMajorThreshold * 0.7f)) {
                return true;
            }

            // Bezel edge trap: touches landing right against screen borders with moderate size
            if (viewWidth > 0 && viewHeight > 0) {
                float marginX = viewWidth * mEdgeMarginFraction;
                float marginY = viewHeight * mEdgeMarginFraction;
                boolean nearEdge = (x <= marginX || x >= (viewWidth - marginX)
                                 || y <= marginY || y >= (viewHeight - marginY));
                if (nearEdge && (touchMajor >= (mPalmTouchMajorThreshold * 0.6f) || size >= (mPalmSizeThreshold * 0.6f))) {
                    return true;
                }
            }
        }

        return false;
    }

    /**
     * MotionEvent adapter. Extracts pointer telemetry and delegates to pure decision core.
     */
    public boolean shouldReject(MotionEvent event, int pointerIndex, float viewWidth, float viewHeight) {
        if (event == null || pointerIndex < 0 || pointerIndex >= event.getPointerCount()) {
            return false;
        }

        long nowMs = SystemClock.uptimeMillis();
        int toolType = event.getToolType(pointerIndex);
        int actionMasked = event.getActionMasked();
        float x = event.getX(pointerIndex);
        float y = event.getY(pointerIndex);
        float touchMajor = event.getTouchMajor(pointerIndex);
        float touchMinor = event.getTouchMinor(pointerIndex);
        float size = event.getSize(pointerIndex);

        int classification = 0;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            classification = event.getClassification();
        }

        return shouldRejectPointer(
                nowMs, toolType, actionMasked, x, y,
                touchMajor, touchMinor, size, classification,
                viewWidth, viewHeight
        );
    }
}
