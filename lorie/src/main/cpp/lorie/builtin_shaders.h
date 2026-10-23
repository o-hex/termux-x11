#ifndef BUILTIN_SHADERS_H
#define BUILTIN_SHADERS_H

// 1. AMD FidelityFX Super Resolution 1.0 (EASU + RCAS edge-directed scaling)
static const char BUILTIN_SHADER_FSR[] = R"glsl(
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

#ifndef SWIZZLE
#define SWIZZLE
#endif

varying vec2 outTexCoords;
uniform sampler2D texture;
uniform vec2 u_tex_size;
uniform vec2 u_texel_size;

void FsrEasuTap(
    inout vec3 aC,
    inout float aW,
    vec2 off,
    vec2 dir,
    vec2 len,
    float lob,
    float clp,
    vec3 c
) {
    vec2 v;
    v.x = (off.x * ( dir.x)) + (off.y * dir.y);
    v.y = (off.x * (-dir.y)) + (off.y * dir.x);
    v *= len;
    float d2 = v.x * v.x + v.y * v.y;
    d2 = min(d2, clp);
    float wB = 0.4 * d2 - 1.0;
    float wA = lob * d2 - 1.0;
    wB *= wB;
    wA *= wA;
    wB = 1.5625 * wB - 0.5625;
    float w = wB * wA;
    aC += c * w;
    aW += w;
}

void FsrEasuSet(
    inout vec2 dir,
    inout float len,
    vec2 pp,
    bool biS, bool biT, bool biU, bool biV,
    float lA, float lB, float lC, float lD, float lE
) {
    float w = 0.0;
    if (biS) w = (1.0 - pp.x) * (1.0 - pp.y);
    if (biT) w =        pp.x  * (1.0 - pp.y);
    if (biU) w = (1.0 - pp.x) *        pp.y;
    if (biV) w =        pp.x  *        pp.y;

    float dc = lD - lC;
    float cb = lC - lB;
    float lenX = max(abs(dc), abs(cb));
    lenX = 1.0 / max(lenX, 0.0001);
    float dirX = lD - lB;
    dir.x += dirX * w;
    lenX = clamp(abs(dirX) * lenX, 0.0, 1.0);
    lenX *= lenX;
    len += lenX * w;

    float ec = lE - lC;
    float ca = lC - lA;
    float lenY = max(abs(ec), abs(ca));
    lenY = 1.0 / max(lenY, 0.0001);
    float dirY = lE - lA;
    dir.y += dirY * w;
    lenY = clamp(abs(dirY) * lenY, 0.0, 1.0);
    lenY *= lenY;
    len += lenY * w;
}

void main(void) {
    vec2 pp = outTexCoords * u_tex_size - 0.5;
    vec2 fp = floor(pp);
    pp -= fp;

    vec2 base = (fp + 0.5) * u_texel_size;
    vec3 b = texture2D(texture, base + vec2( 0.0, -1.0) * u_texel_size).rgb;
    vec3 c = texture2D(texture, base + vec2( 1.0, -1.0) * u_texel_size).rgb;
    vec3 e = texture2D(texture, base + vec2(-1.0,  0.0) * u_texel_size).rgb;
    vec3 f = texture2D(texture, base + vec2( 0.0,  0.0) * u_texel_size).rgb;
    vec3 g = texture2D(texture, base + vec2( 1.0,  0.0) * u_texel_size).rgb;
    vec3 h = texture2D(texture, base + vec2( 2.0,  0.0) * u_texel_size).rgb;
    vec3 i = texture2D(texture, base + vec2(-1.0,  1.0) * u_texel_size).rgb;
    vec3 j = texture2D(texture, base + vec2( 0.0,  1.0) * u_texel_size).rgb;
    vec3 k = texture2D(texture, base + vec2( 1.0,  1.0) * u_texel_size).rgb;
    vec3 l = texture2D(texture, base + vec2( 2.0,  1.0) * u_texel_size).rgb;
    vec3 n = texture2D(texture, base + vec2( 0.0,  2.0) * u_texel_size).rgb;
    vec3 o = texture2D(texture, base + vec2( 1.0,  2.0) * u_texel_size).rgb;

    float bL = b.b * 0.5 + (b.r * 0.5 + b.g);
    float cL = c.b * 0.5 + (c.r * 0.5 + c.g);
    float iL = i.b * 0.5 + (i.r * 0.5 + i.g);
    float jL = j.b * 0.5 + (j.r * 0.5 + j.g);
    float fL = f.b * 0.5 + (f.r * 0.5 + f.g);
    float eL = e.b * 0.5 + (e.r * 0.5 + e.g);
    float kL = k.b * 0.5 + (k.r * 0.5 + k.g);
    float lL = l.b * 0.5 + (l.r * 0.5 + l.g);
    float hL = h.b * 0.5 + (h.r * 0.5 + h.g);
    float gL = g.b * 0.5 + (g.r * 0.5 + g.g);
    float oL = o.b * 0.5 + (o.r * 0.5 + o.g);
    float nL = n.b * 0.5 + (n.r * 0.5 + n.g);

    vec2 dir = vec2(0.0);
    float len = 0.0;
    FsrEasuSet(dir, len, pp, true,  false, false, false, bL, eL, fL, gL, jL);
    FsrEasuSet(dir, len, pp, false, true,  false, false, cL, fL, gL, hL, kL);
    FsrEasuSet(dir, len, pp, false, false, true,  false, fL, iL, jL, kL, nL);
    FsrEasuSet(dir, len, pp, false, false, false, true,  gL, jL, kL, lL, oL);

    float dirR = dir.x * dir.x + dir.y * dir.y;
    if (dirR < 0.00003051757) {
        dir = vec2(1.0, 0.0);
    } else {
        dir *= inversesqrt(dirR);
    }

    len = len * 0.5;
    len *= len;
    float stretch = (dir.x * dir.x + dir.y * dir.y) / max(max(abs(dir.x), abs(dir.y)), 0.0001);
    vec2 len2 = vec2(1.0 + (stretch - 1.0) * len, 1.0 - 0.5 * len);
    float lob = 0.5 + ((0.25 - 0.04) - 0.5) * len;
    float clp = 1.0 / max(lob, 0.0001);

    vec3 min4 = min(min(f, g), min(j, k));
    vec3 max4 = max(max(f, g), max(j, k));

    vec3 aC = vec3(0.0);
    float aW = 0.0;
    FsrEasuTap(aC, aW, vec2( 0.0, -1.0) - pp, dir, len2, lob, clp, b);
    FsrEasuTap(aC, aW, vec2( 1.0, -1.0) - pp, dir, len2, lob, clp, c);
    FsrEasuTap(aC, aW, vec2(-1.0,  1.0) - pp, dir, len2, lob, clp, i);
    FsrEasuTap(aC, aW, vec2( 0.0,  1.0) - pp, dir, len2, lob, clp, j);
    FsrEasuTap(aC, aW, vec2( 0.0,  0.0) - pp, dir, len2, lob, clp, f);
    FsrEasuTap(aC, aW, vec2(-1.0,  0.0) - pp, dir, len2, lob, clp, e);
    FsrEasuTap(aC, aW, vec2( 1.0,  1.0) - pp, dir, len2, lob, clp, k);
    FsrEasuTap(aC, aW, vec2( 2.0,  1.0) - pp, dir, len2, lob, clp, l);
    FsrEasuTap(aC, aW, vec2( 2.0,  0.0) - pp, dir, len2, lob, clp, h);
    FsrEasuTap(aC, aW, vec2( 1.0,  0.0) - pp, dir, len2, lob, clp, g);
    FsrEasuTap(aC, aW, vec2( 1.0,  2.0) - pp, dir, len2, lob, clp, o);
    FsrEasuTap(aC, aW, vec2( 0.0,  2.0) - pp, dir, len2, lob, clp, n);

    vec3 pix = clamp(aC / max(aW, 0.0001), min4, max4);
    gl_FragColor = vec4(pix, 1.0) SWIZZLE;
}
)glsl";

// 2. Hardware Bicubic Catmull-Rom + Contrast-Adaptive Sharpening (BiCAS)
static const char BUILTIN_SHADER_BICAS[] = R"glsl(
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

#ifndef SWIZZLE
#define SWIZZLE
#endif

varying vec2 outTexCoords;
uniform sampler2D texture;
uniform vec2 u_tex_size;
uniform vec2 u_texel_size;
uniform vec2 u_dst_size;
uniform float u_sharpness;

vec4 sampleBicubic(sampler2D tex, vec2 uv) {
    vec2 coord = uv * u_tex_size - 0.5;
    vec2 f = fract(coord);
    vec2 p = floor(coord);

    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = -0.5 * f3 + f2 - 0.5 * f;
    vec2 w1 =  1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w2 = -1.5 * f3 + 2.0 * f2 + 0.5 * f;
    vec2 w3 =  0.5 * f3 - 0.5 * f2;

    vec2 g0 = w0 + w1;
    vec2 g1 = w2 + w3;

    vec2 tc0 = (p - 0.5 + w1 / max(g0, vec2(0.0001))) * u_texel_size;
    vec2 tc1 = (p + 1.5 + w3 / max(g1, vec2(0.0001))) * u_texel_size;

    vec4 s0 = texture2D(tex, vec2(tc0.x, tc0.y));
    vec4 s1 = texture2D(tex, vec2(tc1.x, tc0.y));
    vec4 s2 = texture2D(tex, vec2(tc0.x, tc1.y));
    vec4 s3 = texture2D(tex, vec2(tc1.x, tc1.y));

    float w00 = g0.x * g0.y;
    float w10 = g1.x * g0.y;
    float w01 = g0.x * g1.y;
    float w11 = g1.x * g1.y;

    return s0 * w00 + s1 * w10 + s2 * w01 + s3 * w11;
}

void main() {
    vec4 c = sampleBicubic(texture, outTexCoords);

    vec2 offX = vec2(u_texel_size.x * 0.5, 0.0);
    vec2 offY = vec2(0.0, u_texel_size.y * 0.5);

    vec3 a = texture2D(texture, outTexCoords - offY).rgb;
    vec3 b = texture2D(texture, outTexCoords - offX).rgb;
    vec3 d = texture2D(texture, outTexCoords + offX).rgb;
    vec3 e = texture2D(texture, outTexCoords + offY).rgb;

    vec3 min_rgb = min(min(min(a, b), min(d, e)), c.rgb);
    vec3 max_rgb = max(max(max(a, b), max(d, e)), c.rgb);

    vec3 amp_rgb = clamp(min(min_rgb, 2.0 - max_rgb) / max(max_rgb, vec3(0.0001)), 0.0, 1.0);
    
    float sharp = (u_sharpness <= 0.0) ? 1.0 : u_sharpness;
    vec3 w = -sqrt(amp_rgb) * (sharp * 0.18 + 0.04);
    w = max(w, vec3(-0.35));

    vec3 res = (c.rgb + (a + b + d + e) * w) / (1.0 + 4.0 * w);
    vec3 clamped_res = clamp(res, min_rgb * 0.95, max_rgb * 1.05);

    gl_FragColor = vec4(clamp(clamped_res, 0.0, 1.0), c.a) SWIZZLE;
}
)glsl";

// 3. Qualcomm Snapdragon Game Super Resolution (SGSR)
static const char BUILTIN_SHADER_SGSR[] = R"glsl(
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

#ifndef SWIZZLE
#define SWIZZLE
#endif

varying vec2 outTexCoords;
uniform sampler2D texture;
uniform vec2 u_tex_size;
uniform vec2 u_texel_size;
uniform vec2 u_dst_size;
uniform float u_sharpness;

float fastLanczos2(float x) {
    float wA = x - 4.0;
    float wB = x * wA - wA;
    wA *= wA;
    return wB * wA;
}

vec2 weightY(float dx, float dy, float c, float std) {
    float x = ((dx * dx) + (dy * dy)) * 0.55 + clamp(abs(c) * std, 0.0, 1.0);
    float w = fastLanczos2(x);
    return vec2(w, w * c);
}

float getLuma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec4 centerSample = texture2D(texture, outTexCoords);
    vec3 color = centerSample.rgb;
    float centerL = getLuma(color);

    vec2 imgCoord = outTexCoords * u_tex_size - 0.5;
    vec2 imgCoordPixel = floor(imgCoord);
    vec2 pl = imgCoord - imgCoordPixel;
    vec2 base = (imgCoordPixel + 0.5) * u_texel_size;

    float l_y  = getLuma(texture2D(texture, base).rgb);
    float l_z  = getLuma(texture2D(texture, base + vec2(0.0, 1.0) * u_texel_size).rgb);
    float r_x  = getLuma(texture2D(texture, base + vec2(1.0, 0.0) * u_texel_size).rgb);
    float r_w  = getLuma(texture2D(texture, base + vec2(1.0, 1.0) * u_texel_size).rgb);

    float edgeVote = abs(l_z - l_y) + abs(centerL - l_y) + abs(centerL - l_z);
    float edgeThreshold = 8.0 / 255.0;

    if (edgeVote > edgeThreshold) {
        float l_x  = getLuma(texture2D(texture, base + vec2(-1.0, 0.0) * u_texel_size).rgb);
        float l_w  = getLuma(texture2D(texture, base + vec2(-1.0, 1.0) * u_texel_size).rgb);

        float r_y  = getLuma(texture2D(texture, base + vec2(2.0, 0.0) * u_texel_size).rgb);
        float r_z  = getLuma(texture2D(texture, base + vec2(2.0, 1.0) * u_texel_size).rgb);

        float ud_x = getLuma(texture2D(texture, base + vec2(0.0, -1.0) * u_texel_size).rgb);
        float ud_y = getLuma(texture2D(texture, base + vec2(1.0, -1.0) * u_texel_size).rgb);
        float ud_z = getLuma(texture2D(texture, base + vec2(1.0,  2.0) * u_texel_size).rgb);
        float ud_w = getLuma(texture2D(texture, base + vec2(0.0,  2.0) * u_texel_size).rgb);

        float mean = (l_y + l_z + r_x + r_w) * 0.25;

        float dl_x = l_x - mean;  float dl_y = l_y - mean;
        float dl_z = l_z - mean;  float dl_w = l_w - mean;

        float dr_x = r_x - mean;  float dr_y = r_y - mean;
        float dr_z = r_z - mean;  float dr_w = r_w - mean;

        float dud_x = ud_x - mean; float dud_y = ud_y - mean;
        float dud_z = ud_z - mean; float dud_w = ud_w - mean;

        float centerDelta = centerL - mean;

        float sum = (abs(dl_x) + abs(dl_y) + abs(dl_z) + abs(dl_w)) +
                    (abs(dr_x) + abs(dr_y) + abs(dr_z) + abs(dr_w)) +
                    (abs(dud_x) + abs(dud_y) + abs(dud_z) + abs(dud_w));

        float std = 2.181818 / max(sum, 0.0001);

        vec2 aWY = weightY(pl.x,        pl.y + 1.0, dud_x, std);
        aWY     += weightY(pl.x - 1.0,  pl.y + 1.0, dud_y, std);
        aWY     += weightY(pl.x - 1.0,  pl.y - 2.0, dud_z, std);
        aWY     += weightY(pl.x,        pl.y - 2.0, dud_w, std);
        aWY     += weightY(pl.x + 1.0,  pl.y - 1.0, dl_x,  std);
        aWY     += weightY(pl.x,        pl.y - 1.0, dl_y,  std);
        aWY     += weightY(pl.x,        pl.y,       dl_z,  std);
        aWY     += weightY(pl.x + 1.0,  pl.y,       dl_w,  std);
        aWY     += weightY(pl.x - 1.0,  pl.y - 1.0, dr_x,  std);
        aWY     += weightY(pl.x - 2.0,  pl.y - 1.0, dr_y,  std);
        aWY     += weightY(pl.x - 2.0,  pl.y,       dr_z,  std);
        aWY     += weightY(pl.x - 1.0,  pl.y,       dr_w,  std);

        float finalY = aWY.y / max(aWY.x, 0.0001);

        float maxY = max(max(dl_y, dl_z), max(dr_x, dr_w));
        float minY = min(min(dl_y, dl_z), min(dr_x, dr_w));

        float edgeSharpness = (u_sharpness <= 0.0) ? 2.0 : u_sharpness * 2.0;
        finalY = clamp(edgeSharpness * finalY, minY, maxY);

        float deltaY = finalY - centerDelta;
        deltaY = clamp(deltaY, -23.0 / 255.0, 23.0 / 255.0);

        color = clamp(color + vec3(deltaY), 0.0, 1.0);
    }

    gl_FragColor = vec4(color, centerSample.a) SWIZZLE;
}
)glsl";

// 4. NVIDIA Image Scaling (NIS)
static const char BUILTIN_SHADER_NIS[] = R"glsl(
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

#ifndef SWIZZLE
#define SWIZZLE
#endif

varying vec2 outTexCoords;
uniform sampler2D texture;
uniform vec2 u_tex_size;
uniform vec2 u_texel_size;
uniform vec2 u_dst_size;
uniform float u_sharpness;

float getLuma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec4 sampleCatmullRom(sampler2D tex, vec2 uv) {
    vec2 coord = uv * u_tex_size - 0.5;
    vec2 f = fract(coord);
    vec2 p = floor(coord);

    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = -0.5 * f3 + f2 - 0.5 * f;
    vec2 w1 =  1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w2 = -1.5 * f3 + 2.0 * f2 + 0.5 * f;
    vec2 w3 =  0.5 * f3 - 0.5 * f2;

    vec2 g0 = w0 + w1;
    vec2 g1 = w2 + w3;

    vec2 tc0 = (p - 0.5 + w1 / max(g0, vec2(0.0001))) * u_texel_size;
    vec2 tc1 = (p + 1.5 + w3 / max(g1, vec2(0.0001))) * u_texel_size;

    vec4 s0 = texture2D(tex, vec2(tc0.x, tc0.y));
    vec4 s1 = texture2D(tex, vec2(tc1.x, tc0.y));
    vec4 s2 = texture2D(tex, vec2(tc0.x, tc1.y));
    vec4 s3 = texture2D(tex, vec2(tc1.x, tc1.y));

    float w00 = g0.x * g0.y;
    float w10 = g1.x * g0.y;
    float w01 = g0.x * g1.y;
    float w11 = g1.x * g1.y;

    return s0 * w00 + s1 * w10 + s2 * w01 + s3 * w11;
}

float CalcLTIFast(float y0, float y1, float y2, float y3, float y4) {
    float a_min = min(min(y0, y1), y2);
    float a_max = max(max(y0, y1), y2);
    float b_min = min(min(y2, y3), y4);
    float b_max = max(max(y2, y3), y4);

    float a_cont = a_max - a_min;
    float b_cont = b_max - b_min;

    float cont_ratio = max(a_cont, b_cont) / (min(a_cont, b_cont) + 0.0001);
    return clamp(1.0 - (cont_ratio - 1.25) / 0.75, 0.0, 1.0);
}

float EvalUSM(float y0, float y1, float y2, float y3, float y4, float strength, float limit) {
    float y_usm = (-0.6001 * y1 + 1.2002 * y2 - 0.6001 * y3) * strength;
    y_usm = clamp(y_usm, -limit, limit);
    y_usm *= CalcLTIFast(y0, y1, y2, y3, y4);
    return y_usm;
}

void main() {
    vec4 baseColor = sampleCatmullRom(texture, outTexCoords);
    vec3 color = baseColor.rgb;
    float centerLuma = getLuma(color);

    vec2 dx = vec2(u_texel_size.x * 0.5, 0.0);
    vec2 dy = vec2(0.0, u_texel_size.y * 0.5);

    float l_c  = centerLuma;
    float l_l1 = getLuma(texture2D(texture, outTexCoords - dx).rgb);
    float l_l2 = getLuma(texture2D(texture, outTexCoords - 2.0 * dx).rgb);
    float l_r1 = getLuma(texture2D(texture, outTexCoords + dx).rgb);
    float l_r2 = getLuma(texture2D(texture, outTexCoords + 2.0 * dx).rgb);

    float l_t1 = getLuma(texture2D(texture, outTexCoords - dy).rgb);
    float l_t2 = getLuma(texture2D(texture, outTexCoords - 2.0 * dy).rgb);
    float l_b1 = getLuma(texture2D(texture, outTexCoords + dy).rgb);
    float l_b2 = getLuma(texture2D(texture, outTexCoords + 2.0 * dy).rgb);

    vec2 dxy1 = dx + dy;
    vec2 dxy2 = dx - dy;

    float l_tl1 = getLuma(texture2D(texture, outTexCoords - dxy1).rgb);
    float l_tl2 = getLuma(texture2D(texture, outTexCoords - 2.0 * dxy1).rgb);
    float l_br1 = getLuma(texture2D(texture, outTexCoords + dxy1).rgb);
    float l_br2 = getLuma(texture2D(texture, outTexCoords + 2.0 * dxy1).rgb);

    float l_tr1 = getLuma(texture2D(texture, outTexCoords - dxy2).rgb);
    float l_tr2 = getLuma(texture2D(texture, outTexCoords - 2.0 * dxy2).rgb);
    float l_bl1 = getLuma(texture2D(texture, outTexCoords + dxy2).rgb);
    float l_bl2 = getLuma(texture2D(texture, outTexCoords + 2.0 * dxy2).rgb);

    float g_0   = abs(l_l1 - l_r1) + 0.5 * abs(l_l2 - l_r2);
    float g_90  = abs(l_t1 - l_b1) + 0.5 * abs(l_t2 - l_b2);
    float g_45  = abs(l_tr1 - l_bl1) + 0.5 * abs(l_tr2 - l_bl2);
    float g_135 = abs(l_tl1 - l_br1) + 0.5 * abs(l_tl2 - l_br2);

    float g_0_90_max = max(g_0, g_90);
    float g_45_135_max = max(g_45, g_135);
    float sum_g = g_0_90_max + g_45_135_max;

    vec4 dirWeights = vec4(0.25);
    if (sum_g > 0.0001) {
        float e_0_90 = clamp(g_0_90_max / sum_g, 0.0, 1.0);
        float e_45_135 = 1.0 - e_0_90;

        float w0   = (g_0 >= g_90) ? e_0_90 : 0.0;
        float w90  = (g_90 > g_0)  ? e_0_90 : 0.0;
        float w45  = (g_45 >= g_135) ? e_45_135 : 0.0;
        float w135 = (g_135 > g_45)  ? e_45_135 : 0.0;

        dirWeights = vec4(w0, w90, w45, w135);
        float totalW = dirWeights.x + dirWeights.y + dirWeights.z + dirWeights.w;
        if (totalW > 0.0001) {
            dirWeights /= totalW;
        }
    }

    float sharp = (u_sharpness <= 0.0) ? 0.85 : u_sharpness;
    float strength = sharp * 1.5;
    float limit = max(0.2 * sharp, 0.15);

    float usm_0   = EvalUSM(l_l2, l_l1, l_c, l_r1, l_r2, strength, limit);
    float usm_90  = EvalUSM(l_t2, l_t1, l_c, l_b1, l_b2, strength, limit);
    float usm_45  = EvalUSM(l_tr2, l_tr1, l_c, l_bl1, l_bl2, strength, limit);
    float usm_135 = EvalUSM(l_tl2, l_tl1, l_c, l_br1, l_br2, strength, limit);

    float final_usm = dot(vec4(usm_0, usm_90, usm_45, usm_135), dirWeights);
    vec3 result = color + vec3(final_usm);

    gl_FragColor = vec4(clamp(result, 0.0, 1.0), baseColor.a) SWIZZLE;
}
)glsl";

// 5. Robust Contrast-Adaptive Sharpening (RCAS standalone)
static const char BUILTIN_SHADER_RCAS[] = R"glsl(
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

#ifndef SWIZZLE
#define SWIZZLE
#endif

varying vec2 outTexCoords;
uniform sampler2D texture;
uniform vec2 u_texel_size;
uniform float u_sharpness;

void main(void) {
    vec2 sp = outTexCoords;
    vec3 b = texture2D(texture, sp + vec2( 0.0, -u_texel_size.y)).rgb;
    vec3 d = texture2D(texture, sp + vec2(-u_texel_size.x,  0.0)).rgb;
    vec4 ee = texture2D(texture, sp);
    vec3 e = ee.rgb;
    vec3 f = texture2D(texture, sp + vec2( u_texel_size.x,  0.0)).rgb;
    vec3 h = texture2D(texture, sp + vec2( 0.0,  u_texel_size.y)).rgb;

    float bL = b.b * 0.5 + (b.r * 0.5 + b.g);
    float dL = d.b * 0.5 + (d.r * 0.5 + d.g);
    float eL = e.b * 0.5 + (e.r * 0.5 + e.g);
    float fL = f.b * 0.5 + (f.r * 0.5 + f.g);
    float hL = h.b * 0.5 + (h.r * 0.5 + h.g);

    float nz = 0.25 * bL + 0.25 * dL + 0.25 * fL + 0.25 * hL - eL;
    float range = max(max(max(bL, dL), max(eL, fL)), hL) - min(min(min(bL, dL), min(eL, fL)), hL);
    nz = clamp(abs(nz) / max(range, 0.0001), 0.0, 1.0);
    nz = -0.5 * nz + 1.0;

    vec3 mn4 = min(min(min(b, d), f), h);
    vec3 mx4 = max(max(max(b, d), f), h);

    vec3 hitMin = min(mn4, e) / (4.0 * mx4 + 0.0001);
    vec3 hitMax = (1.0 - max(mx4, e)) / (4.0 * mn4 - 4.0 + 0.0001);
    vec3 lobe = max(-hitMin, hitMax);
    float maxLobe = max(max(lobe.r, lobe.g), lobe.b);

    float sharp = (u_sharpness <= 0.0) ? 0.8 : u_sharpness;
    float l = max(-0.1875, min(maxLobe, 0.0)) * sharp * nz;
    float rcpL = 1.0 / (4.0 * l + 1.0);

    vec3 pix = (l * (b + d + f + h) + e) * rcpL;
    gl_FragColor = vec4(clamp(pix, 0.0, 1.0), ee.a) SWIZZLE;
}
)glsl";

// 6. Elliptical Weighted Average / Separable Anti-Ringing Lanczos-3
static const char BUILTIN_SHADER_LANCZOS[] = R"glsl(
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

#ifndef SWIZZLE
#define SWIZZLE
#endif

varying vec2 outTexCoords;
uniform sampler2D texture;
uniform vec2 u_tex_size;
uniform vec2 u_texel_size;

float sinc(float x) {
    if (abs(x) < 0.0001) return 1.0;
    x *= 3.14159265359;
    return sin(x) / x;
}

float lanczos3(float x) {
    if (abs(x) >= 3.0) return 0.0;
    return sinc(x) * sinc(x / 3.0);
}

void main() {
    vec2 pos = outTexCoords * u_tex_size - 0.5;
    vec2 f = fract(pos);
    vec2 base = (floor(pos) + 0.5) * u_texel_size;

    vec4 color = vec4(0.0);
    float totalWeight = 0.0;

    vec4 c00 = texture2D(texture, base);
    vec4 c10 = texture2D(texture, base + vec2(u_texel_size.x, 0.0));
    vec4 c01 = texture2D(texture, base + vec2(0.0, u_texel_size.y));
    vec4 c11 = texture2D(texture, base + u_texel_size);

    vec4 minColor = min(min(c00, c10), min(c01, c11));
    vec4 maxColor = max(max(c00, c10), max(c01, c11));

    for (int y = -1; y <= 2; y++) {
        float wy = lanczos3(float(y) - f.y);
        for (int x = -1; x <= 2; x++) {
            float wx = lanczos3(float(x) - f.x);
            float w = wx * wy;
            vec2 offset = vec2(float(x), float(y)) * u_texel_size;
            color += texture2D(texture, base + offset) * w;
            totalWeight += w;
        }
    }

    color /= max(totalWeight, 0.0001);
    color = clamp(color, minColor, maxColor);

    gl_FragColor = clamp(color, 0.0, 1.0) SWIZZLE;
}
)glsl";

#endif // BUILTIN_SHADERS_H
