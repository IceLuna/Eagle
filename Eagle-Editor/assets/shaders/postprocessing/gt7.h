// From Gran Turismo 7
// Source: https://blog.selfshadow.com/publications/s2025-shading-course/pdi/s2025_pbs_pdi_slides_v1.1.pdf

#ifndef EG_POSTPROCESSING_GT7
#define EG_POSTPROCESSING_GT7

// -----------------------------------------------------------------------------
// Defines the SDR reference white level used in our tone mapping (typically 250 nits).
// -----------------------------------------------------------------------------
#define GRAN_TURISMO_SDR_PAPER_WHITE 250.0f // cd/m^2

// -----------------------------------------------------------------------------
// Gran Turismo luminance-scale conversion helpers.
// In Gran Turismo, 1.0f in the linear frame-buffer space corresponds to
// REFERENCE_LUMINANCE cd/m^2 of physical luminance (typically 100 cd/m^2).
// -----------------------------------------------------------------------------
#define GT7_REFERENCE_LUMINANCE 100.0f // cd/m^2 <-> 1.0f

#define TONE_MAPPING_UCS_ICTCP  0
#define TONE_MAPPING_UCS_JZAZBZ 1
#define TONE_MAPPING_UCS        TONE_MAPPING_UCS_ICTCP

float GT7_physicalValueToFrameBufferValue(float physical)
{
    // Converts physical luminance (cd/m^2) to a linear frame-buffer value,
    // where 1.0 corresponds to REFERENCE_LUMINANCE (e.g., 100 cd/m^2).
    return physical / GT7_REFERENCE_LUMINANCE;
}

float GT7_frameBufferValueToPhysicalValue(float fbValue)
{
    // Converts linear frame-buffer value to physical luminance (cd/m^2)
    // where 1.0 corresponds to REFERENCE_LUMINANCE (e.g., 100 cd/m^2).
    return fbValue * GT7_REFERENCE_LUMINANCE;
}

// -----------------------------------------------------------------------------
// Unified color space (UCS): ICtCp or Jzazbz.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// EOTF / inverse-EOTF for ST-2084 (PQ).
// Note: Introduce exponentScaleFactor to allow scaling of the exponent in the EOTF for Jzazbz.
// -----------------------------------------------------------------------------
float GT7_eotfSt2084(float n, float exponentScaleFactor)
{
    if (n < 0.0f)
    {
        n = 0.0f;
    }
    if (n > 1.0f)
    {
        n = 1.0f;
    }

    // Base functions from SMPTE ST 2084:2014
    // Converts from normalized PQ (0-1) to absolute luminance in cd/m^2 (linear light)
    // Assumes float input; does not handle integer encoding (Annex)
    // Assumes full-range signal (0-1)
    const float m1 = 0.1593017578125f;                // (2610 / 4096) / 4
    const float m2 = 78.84375f * exponentScaleFactor; // (2523 / 4096) * 128
    const float c1 = 0.8359375f;                      // 3424 / 4096
    const float c2 = 18.8515625f;                     // (2413 / 4096) * 32
    const float c3 = 18.6875f;                        // (2392 / 4096) * 32
    const float pqC = 10000.0f;                        // Maximum luminance supported by PQ (cd/m^2)

    // Does not handle signal range from 2084 - assumes full range (0-1)
    float np = pow(n, 1.0f / m2);
    float l = np - c1;

    if (l < 0.0f)
    {
        l = 0.0f;
    }

    l = l / (c2 - c3 * np);
    l = pow(l, 1.0f / m1);

    // Convert absolute luminance (cd/m^2) into the frame-buffer linear scale.
    return GT7_physicalValueToFrameBufferValue(l * pqC);
}

float GT7_inverseEotfSt2084(float v, float exponentScaleFactor)
{
    const float m1 = 0.1593017578125f;
    const float m2 = 78.84375f * exponentScaleFactor;
    const float c1 = 0.8359375f;
    const float c2 = 18.8515625f;
    const float c3 = 18.6875f;
    const float pqC = 10000.0f;

    // Convert the frame-buffer linear scale into absolute luminance (cd/m^2).
    float physical = GT7_frameBufferValueToPhysicalValue(v);
    float y = physical / pqC; // Normalize for the ST-2084 curve

    float ym = pow(y, m1);
    return exp2(m2 * (log2(c1 + c2 * ym) - log2(1.0f + c3 * ym)));
}

void GT7_rgbToICtCp(vec3 rgb, out vec3 ictCp) // Input: linear Rec.2020
{
    float l = (rgb[0] * 1688.0f + rgb[1] * 2146.0f + rgb[2] * 262.0f) / 4096.0f;
    float m = (rgb[0] * 683.0f + rgb[1] * 2951.0f + rgb[2] * 462.0f) / 4096.0f;
    float s = (rgb[0] * 99.0f + rgb[1] * 309.0f + rgb[2] * 3688.0f) / 4096.0f;

    float lPQ = GT7_inverseEotfSt2084(l, 1.0);
    float mPQ = GT7_inverseEotfSt2084(m, 1.0);
    float sPQ = GT7_inverseEotfSt2084(s, 1.0);

    ictCp[0] = (2048.0f * lPQ + 2048.0f * mPQ) / 4096.0f;
    ictCp[1] = (6610.0f * lPQ - 13613.0f * mPQ + 7003.0f * sPQ) / 4096.0f;
    ictCp[2] = (17933.0f * lPQ - 17390.0f * mPQ - 543.0f * sPQ) / 4096.0f;
}

void GT7_iCtCpToRgb(vec3 ictCp, out vec3 rgb) // Output: linear Rec.2020
{
    float l = ictCp[0] + 0.00860904f * ictCp[1] + 0.11103f * ictCp[2];
    float m = ictCp[0] - 0.00860904f * ictCp[1] - 0.11103f * ictCp[2];
    float s = ictCp[0] + 0.560031f * ictCp[1] - 0.320627f * ictCp[2];

    float lLin = GT7_eotfSt2084(l, 1.0);
    float mLin = GT7_eotfSt2084(m, 1.0);
    float sLin = GT7_eotfSt2084(s, 1.0);

    rgb[0] = max(3.43661f * lLin - 2.50645f * mLin + 0.0698454f * sLin, 0.0f);
    rgb[1] = max(-0.79133f * lLin + 1.9836f * mLin - 0.192271f * sLin, 0.0f);
    rgb[2] = max(-0.0259499f * lLin - 0.0989137f * mLin + 1.12486f * sLin, 0.0f);
}

// -----------------------------------------------------------------------------
// Jzazbz conversion.
// Reference:
// Muhammad Safdar, Guihua Cui, Youn Jin Kim, and Ming Ronnier Luo,
// "Perceptually uniform color space for image signals including high dynamic
// range and wide gamut," Opt. Express 25, 15131-15151 (2017)
// Note: Coefficients adjusted for linear Rec.2020
// -----------------------------------------------------------------------------
#define JZAZBZ_EXPONENT_SCALE_FACTOR 1.7f // Scale factor for exponent

void GT7_rgbToJzazbz(vec3 rgb, out vec3 jab) // Input: linear Rec.2020
{
    float l = rgb[0] * 0.530004f + rgb[1] * 0.355704f + rgb[2] * 0.086090f;
    float m = rgb[0] * 0.289388f + rgb[1] * 0.525395f + rgb[2] * 0.157481f;
    float s = rgb[0] * 0.091098f + rgb[1] * 0.147588f + rgb[2] * 0.734234f;

    float lPQ = GT7_inverseEotfSt2084(l, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float mPQ = GT7_inverseEotfSt2084(m, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float sPQ = GT7_inverseEotfSt2084(s, JZAZBZ_EXPONENT_SCALE_FACTOR);

    float iz = 0.5f * lPQ + 0.5f * mPQ;

    jab[0] = (0.44f * iz) / (1.0f - 0.56f * iz) - 1.6295499532821566e-11f;
    jab[1] = 3.524000f * lPQ - 4.066708f * mPQ + 0.542708f * sPQ;
    jab[2] = 0.199076f * lPQ + 1.096799f * mPQ - 1.295875f * sPQ;
}

void GT7_jzazbzToRgb(vec3 jab, out vec3 rgb) // Output: linear Rec.2020
{
    float jz = jab[0] + 1.6295499532821566e-11f;
    float iz = jz / (0.44f + 0.56f * jz);
    float a = jab[1];
    float b = jab[2];

    float l = iz + a * 1.386050432715393e-1f + b * 5.804731615611869e-2f;
    float m = iz + a * -1.386050432715393e-1f + b * -5.804731615611869e-2f;
    float s = iz + a * -9.601924202631895e-2f + b * -8.118918960560390e-1f;

    float lLin = GT7_eotfSt2084(l, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float mLin = GT7_eotfSt2084(m, JZAZBZ_EXPONENT_SCALE_FACTOR);
    float sLin = GT7_eotfSt2084(s, JZAZBZ_EXPONENT_SCALE_FACTOR);

    rgb[0] = lLin * 2.990669f + mLin * -2.049742f + sLin * 0.088977f;
    rgb[1] = lLin * -1.634525f + mLin * 3.145627f + sLin * -0.483037f;
    rgb[2] = lLin * -0.042505f + mLin * -0.377983f + sLin * 1.448019f;
}

#if TONE_MAPPING_UCS == TONE_MAPPING_UCS_ICTCP
void GT7_rgbToUcs(vec3 rgb, out vec3 ucs)
{
    GT7_rgbToICtCp(rgb, ucs);
}
void GT7_ucsToRgb(vec3 ucs, out vec3 rgb)
{
    GT7_iCtCpToRgb(ucs, rgb);
}
#elif TONE_MAPPING_UCS == TONE_MAPPING_UCS_JZAZBZ
void GT7_rgbToUcs(vec3 rgb, out vec3 ucs)
{
    GT7_rgbToJzazbz(rgb, ucs);
}
void GT7_ucsToRgb(vec3 ucs, out vec3 rgb)
{
    GT7_jzazbzToRgb(ucs, rgb);
}
#else
#error "Unsupported TONE_MAPPING_UCS value. Please define TONE_MAPPING_UCS as either TONE_MAPPING_UCS_ICTCP or TONE_MAPPING_UCS_JZAZBZ."
#endif

struct GT7_Params
{
    float framebufferLuminanceTarget;
    float framebufferLuminanceTargetUcs;
    float blendRatio;
    float fadeStart;
    float fadeEnd;
    float sdrCorrectionFactor;
};

struct GT7_Curve
{
    float peakIntensity;
    float alpha;
    float midPoint;
    float linearSection;
    float toeStrength;
    float kA;
    float kB;
    float kC;
};

GT7_Curve GT7_initializeCurve(float monitorIntensity, float alpha, float grayPoint, float linearSection, float toeStrength)
{
    GT7_Curve curve;

    curve.peakIntensity = monitorIntensity;
    curve.alpha = alpha;
    curve.midPoint = grayPoint;
    curve.linearSection = linearSection;
    curve.toeStrength = toeStrength;

    // Pre-compute constants for the shoulder region.
    const float k = (curve.linearSection - 1.0f) / (curve.alpha - 1.0f);
    curve.kA = curve.peakIntensity * curve.linearSection + curve.peakIntensity * k;
    curve.kB = -curve.peakIntensity * k * exp(curve.linearSection / k);
    curve.kC = -1.0f / (k * curve.peakIntensity);

    return curve;
}

float GT7_smoothStep(float x, float edge0, float edge1)
{
    float t = (x - edge0) / (edge1 - edge0);

    if (x < edge0)
    {
        return 0.0f;
    }
    if (x > edge1)
    {
        return 1.0f;
    }

    return t * t * (3.0f - 2.0f * t);
}

float GT7_evaluateCurve(GT7_Curve curve, float x)
{
    if (x < 0.0f)
    {
        return 0.0f;
    }

    float weightLinear = GT7_smoothStep(x, 0.0f, curve.midPoint);
    float weightToe = 1.0f - weightLinear;

    // Shoulder mapping for highlights.
    float shoulder = curve.kA + curve.kB * exp(x * curve.kC);

    if (x < curve.linearSection * curve.peakIntensity)
    {
        float toeMapped = curve.midPoint * pow(x / curve.midPoint, curve.toeStrength);
        return weightToe * toeMapped + weightLinear * x;
    }
    else
    {
        return shoulder;
    }
}

float GT7_chromaCurve(float x, float a, float b)
{
    return 1.0f - GT7_smoothStep(x, a, b);
}

vec3 GT7_applyToneMapping(vec3 rgb, GT7_Params params, GT7_Curve curve)
{
    // Convert to UCS to separate luminance and chroma.
    vec3 ucs;
    GT7_rgbToUcs(rgb, ucs);

    // Per-channel tone mapping ("skewed" color).
    vec3 skewedRgb = vec3(GT7_evaluateCurve(curve, rgb[0]),
        GT7_evaluateCurve(curve, rgb[1]),
        GT7_evaluateCurve(curve, rgb[2]));

    vec3 skewedUcs;
    GT7_rgbToUcs(skewedRgb, skewedUcs);

    float chromaScale = GT7_chromaCurve(ucs[0] / params.framebufferLuminanceTargetUcs, params.fadeStart, params.fadeEnd);

    const vec3 scaledUcs = vec3(skewedUcs[0],         // Luminance from skewed color
        ucs[1] * chromaScale, // Scaled chroma components
        ucs[2] * chromaScale);

    // Convert back to RGB.
    vec3 scaledRgb;
    GT7_ucsToRgb(scaledUcs, scaledRgb);

    // Final blend between per-channel and UCS-scaled results.
    vec3 result;
    for (int i = 0; i < 3; ++i)
    {
        float blended = (1.0f - params.blendRatio) * skewedRgb[i] + params.blendRatio * scaledRgb[i];
        // When using SDR, apply the correction factor.
        // When using HDR, sdrCorrectionFactor is 1.0f, so it has no effect.
        result[i] = params.sdrCorrectionFactor * min(blended, params.framebufferLuminanceTarget);
    }

    return result;
}

// Initializes the tone mapping curve and related parameters based on the target display luminance.
// This method should not be called directly. Use initializeAsHDR() or initializeAsSDR() instead.
void GT7_initializeParameters(float physicalTargetLuminance, inout GT7_Params params, out GT7_Curve curve)
{
    params.framebufferLuminanceTarget = GT7_physicalValueToFrameBufferValue(physicalTargetLuminance);

    // Initialize the curve (slightly different parameters from GT Sport).
    curve = GT7_initializeCurve(params.framebufferLuminanceTarget, 0.25f, 0.538f, 0.444f, 1.280f);

    // Default parameters.
    params.blendRatio = 0.6f;
    params.fadeStart = 0.98f;
    params.fadeEnd = 1.16f;

    vec3 ucs;
    vec3 rgb = vec3(params.framebufferLuminanceTarget);

    GT7_rgbToUcs(rgb, ucs);

    params.framebufferLuminanceTargetUcs = ucs[0]; // Use the first UCS component (I or Jz) as luminance
}

// Initialize for SDR (Standard Dynamic Range) display.
void GT7_initializeAsSDR(inout GT7_Params params, out GT7_Curve curve)
{
    // Regarding SDR output:
    // First, in GT (Gran Turismo), it is assumed that a maximum value of 1.0 in SDR output
    // corresponds to GRAN_TURISMO_SDR_PAPER_WHITE (typically 250 nits).
    // Therefore, tone mapping for SDR output is performed based on GRAN_TURISMO_SDR_PAPER_WHITE.
    // However, in the sRGB standard, 1.0f corresponds to 100 nits,
    // so we need to "undo" the tone-mapped values accordingly.
    // To match the sRGB range, the tone-mapped values are scaled using sdrCorrectionFactor_.
    //
    // * These adjustments ensure that the visual appearance (in terms of brightness)
    //   stays generally consistent across both HDR and SDR outputs for the same rendered content.
    params.sdrCorrectionFactor = 1.0f / GT7_physicalValueToFrameBufferValue(GRAN_TURISMO_SDR_PAPER_WHITE);
    GT7_initializeParameters(GRAN_TURISMO_SDR_PAPER_WHITE, params, curve);
}

vec3 GT7(vec3 color)
{
    GT7_Params params;
    GT7_Curve curve;
    GT7_initializeAsSDR(params, curve);

    return GT7_applyToneMapping(color, params, curve);
}

#endif // EG_POSTPROCESSING_GT7
