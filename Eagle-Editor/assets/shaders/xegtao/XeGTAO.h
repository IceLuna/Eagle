#ifndef XE_GTAO
#define XE_GTAO

#include "utils.h"

#define XE_GTAO_FP32_DEPTHS
#define XE_GTAO_DEPTH_MIP_LEVELS 0
#define XE_GTAO_USE_DEFAULT_CONSTANTS 0

#define XE_GTAO_DEFAULT_RADIUS_MULTIPLIER               (1.457f)  // allows us to use different value as compared to ground truth radius to counter inherent screen space biases
#define XE_GTAO_DEFAULT_FALLOFF_RANGE                   (0.615f)  // distant samples contribute less
#define XE_GTAO_DEFAULT_SAMPLE_DISTRIBUTION_POWER       (2.0f  )  // small crevices more important than big surfaces
#define XE_GTAO_DEFAULT_THIN_OCCLUDER_COMPENSATION      (0.0f  )  // the new 'thickness heuristic' approach
#define XE_GTAO_DEFAULT_FINAL_VALUE_POWER               (2.2f  )  // modifies the final ambient occlusion value using power function - this allows some of the above heuristics to do different things
#define XE_GTAO_DEFAULT_DEPTH_MIP_SAMPLING_OFFSET       (3.30f )  // main trade-off between performance (memory bandwidth) and quality (temporal stability is the first affected, thin objects next)
#define XE_GTAO_OCCLUSION_TERM_SCALE                    (1.0f)    // for packing in UNORM (because raw, pre-denoised occlusion term can overshoot 1 but will later average out to 1)

#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
#define AOTermType vec4            // .xyz is bent normal, .w is visibility term
#else
#define AOTermType float           // .x is visibility term
#endif

struct GTAOConstants
{
	ivec2 ViewportSize;
	vec2  ViewportPixelSize;                  // .zw == 1.0 / ViewportSize.xy

    vec2  CameraPlanes; // Near; Far
	vec2  CameraTanHalfFOV;

	vec2  NDCToViewMul;
	vec2  NDCToViewAdd;

	vec2  NDCToViewMul_x_PixelSize;
	float EffectRadius;                       // world (viewspace) maximum size of the shadow
	float EffectFalloffRange;

	float RadiusMultiplier;
	float FinalValuePower;
	float DenoiseBlurBeta;
    uint  FinalPass;

	float SampleDistributionPower;
	float ThinOccluderCompensation;
	float DepthMIPSamplingOffset;
	int   NoiseIndex;                         // frameIndex % 64 if using TAA or 0 otherwise
};

vec2 SpatioTemporalNoise(usampler2D hilbertCurve, ivec2 pixCoord, uint temporalIndex) // without TAA, temporalIndex is always 0
{
    uint index = texelFetch(hilbertCurve, pixCoord % 64, 0).x;
    index += 288 * (temporalIndex % 64); // why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)
    // R2 sequence - see http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
    return vec2(fract(0.5 + index * vec2(0.75487766624669276005, 0.5698402909980532659114)));
}

vec4 XeGTAO_R8G8B8A8_UNORM_to_FLOAT4(uint packedInput)
{
    vec4 unpackedOutput;
    unpackedOutput.x = (packedInput & 0x000000ff) / 255.0;
    unpackedOutput.y = (((packedInput >> 8) & 0x000000ff)) / 255.0;
    unpackedOutput.z = (((packedInput >> 16) & 0x000000ff)) / 255.0;
    unpackedOutput.w = (packedInput >> 24) / 255.0;
    return unpackedOutput;
}

uint XeGTAO_FLOAT4_to_R8G8B8A8_UNORM(vec4 unpackedInput)
{
    return ((uint(clamp(unpackedInput.x, 0.0, 1.0) * 255.0 + 0.5)) |
        (uint(clamp(unpackedInput.y, 0.0, 1.0) * 255.0 + 0.5) << 8) |
        (uint(clamp(unpackedInput.z, 0.0, 1.0) * 255.0 + 0.5) << 16) |
        (uint(clamp(unpackedInput.w, 0.0, 1.0) * 255.0 + 0.5) << 24));
}

void XeGTAO_DecodeVisibilityBentNormal(const uint packedValue, out float visibility, out vec3 bentNormal)
{
    vec4 decoded = XeGTAO_R8G8B8A8_UNORM_to_FLOAT4(packedValue);
    bentNormal = decoded.xyz * 2.0.xxx - 1.0.xxx;   // could normalize - don't want to since it's done so many times, better to do it at the final step only
    visibility = decoded.w;
}

vec4 XeGTAO_UnpackEdges(float _packedVal)
{
    uint packedVal = uint(_packedVal * 255.5);
    vec4 edgesLRTB;
    edgesLRTB.x = float((packedVal >> 6) & 0x03) / 3.0;          // there's really no need for mask (as it's an 8 bit input) but I'll leave it in so it doesn't cause any trouble in the future
    edgesLRTB.y = float((packedVal >> 4) & 0x03) / 3.0;
    edgesLRTB.z = float((packedVal >> 2) & 0x03) / 3.0;
    edgesLRTB.w = float((packedVal >> 0) & 0x03) / 3.0;

    return clamp(edgesLRTB, vec4(0), vec4(1));
}

void XeGTAO_AddSample(AOTermType ssaoValue, float edgeValue, inout AOTermType sum, inout float sumWeight)
{
    float weight = edgeValue;

    sum += (weight * ssaoValue);
    sumWeight += weight;
}

uint XeGTAO_EncodeVisibilityBentNormal(float visibility, vec3 bentNormal)
{
    return XeGTAO_FLOAT4_to_R8G8B8A8_UNORM(vec4(bentNormal * 0.5 + 0.5, visibility));
}

vec4 XeGTAO_CalculateEdges(const float centerZ, const float leftZ, const float rightZ, const float topZ, const float bottomZ)
{
    vec4 edgesLRTB = vec4(leftZ, rightZ, topZ, bottomZ) - centerZ;

    float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
    float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
    vec4 edgesLRTBSlopeAdjusted = edgesLRTB + vec4(slopeLR, -slopeLR, slopeTB, -slopeTB);
    edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));
    return vec4(clamp((1.25 - edgesLRTB / (centerZ * 0.011)), vec4(0), vec4(1)));
}

// Inputs are screen XY and viewspace depth, output is viewspace position
vec3 XeGTAO_ComputeViewspacePosition(const vec2 screenPos, const float viewspaceDepth, const GTAOConstants consts)
{
    vec3 ret;
    ret.xy = (consts.NDCToViewMul * screenPos.xy + consts.NDCToViewAdd) * viewspaceDepth;
    ret.z = viewspaceDepth;
    return ret;
}

vec3 XeGTAO_CalculateNormal(const vec4 edgesLRTB, vec3 pixCenterPos, vec3 pixLPos, vec3 pixRPos, vec3 pixTPos, vec3 pixBPos)
{
    // Get this pixel's viewspace normal
    vec4 acceptedNormals = clamp(vec4(edgesLRTB.x * edgesLRTB.z, edgesLRTB.z * edgesLRTB.y, edgesLRTB.y * edgesLRTB.w, edgesLRTB.w * edgesLRTB.x) + 0.01, vec4(0), vec4(1));

    pixLPos = normalize(pixLPos - pixCenterPos);
    pixRPos = normalize(pixRPos - pixCenterPos);
    pixTPos = normalize(pixTPos - pixCenterPos);
    pixBPos = normalize(pixBPos - pixCenterPos);

    vec3 pixelNormal = acceptedNormals.x * cross(pixLPos, pixTPos) +
        +acceptedNormals.y * cross(pixTPos, pixRPos) +
        +acceptedNormals.z * cross(pixRPos, pixBPos) +
        +acceptedNormals.w * cross(pixBPos, pixLPos);
    pixelNormal = normalize(pixelNormal);

    return pixelNormal;
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float XeGTAO_FastSqrt(float x)
{
    return (intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(x) >> 1)));
}

// input [-1, 1] and output [0, PI], from https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float XeGTAO_FastACos(float inX)
{
    float x = abs(inX);
    float res = -0.156583 * x + EG_HALF_PI;
    res *= XeGTAO_FastSqrt(1.0 - x);
    return (inX >= 0) ? res : EG_PI - res;
}

// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float XeGTAO_PackEdges(vec4 edgesLRTB)
{
    // integer version:
    // edgesLRTB = saturate(edgesLRTB) * 2.9.xxxx + 0.5.xxxx;
    // return (((uint)edgesLRTB.x) << 6) + (((uint)edgesLRTB.y) << 4) + (((uint)edgesLRTB.z) << 2) + (((uint)edgesLRTB.w));
    // 
    // optimized, should be same as above
    edgesLRTB = round(clamp(edgesLRTB, vec4(0), vec4(1)) * 2.9);
    return dot(edgesLRTB, vec4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
}

#ifdef XE_GTAO_MAIN_PASS

// "Efficiently building a matrix to rotate one vector to another"
// http://cs.brown.edu/research/pubs/pdfs/1999/Moller-1999-EBA.pdf / https://dl.acm.org/doi/10.1080/10867651.1999.10487509
// (using https://github.com/assimp/assimp/blob/master/include/assimp/matrix3x3.inl#L275 as a code reference as it seems to be best)
mat3 XeGTAO_RotFromToMatrix(vec3 from, vec3 to)
{
    const float e = dot(from, to);
    const float f = abs(e); //(e < 0)? -e:e;

    // WARNING: This has not been tested/worked through, especially not for 16bit floats; seems to work in our special use case (from is always {0, 0, -1}) but wouldn't use it in general
    if (f > (1.0 - 0.0003))
        return mat3(1, 0, 0, 0, 1, 0, 0, 0, 1);

    const vec3 v = cross(from, to);
    /* ... use this hand optimized version (9 mults less) */
    const float h = (1.0) / (1.0 + e);      /* optimization by Gottfried Chen */
    const float hvx = h * v.x;
    const float hvz = h * v.z;
    const float hvxy = hvx * v.y;
    const float hvxz = hvx * v.z;
    const float hvyz = hvz * v.y;

    mat3 mtx;
    mtx[0][0] = e + hvx * v.x;
    mtx[1][0] = hvxy - v.z;
    mtx[2][0] = hvxz + v.y;

    mtx[0][1] = hvxy + v.z;
    mtx[1][1] = e + h * v.y * v.y;
    mtx[2][1] = hvyz - v.x;

    mtx[0][2] = hvxz - v.y;
    mtx[1][2] = hvyz + v.x;
    mtx[2][2] = e + hvz * v.z;

    return mtx;
}

void XeGTAO_MainPass(
    ivec2 pixCoord,
    float sliceCount,
    float stepsPerSlice,
    vec2 localNoise,
    vec3 viewspaceNormal,
    GTAOConstants consts
)
{
    vec2 normalizedScreenPos = (vec2(pixCoord) + vec2(0.5)) * consts.ViewportPixelSize;

    vec4 valuesUL = textureGatherOffset(g_Depth, vec2(pixCoord) * consts.ViewportPixelSize, ivec2(0), 0);
    vec4 valuesBR = textureGatherOffset(g_Depth, vec2(pixCoord) * consts.ViewportPixelSize, ivec2(1), 0);
    for (int i = 0; i < 4; ++i)
    {
        valuesUL[i] = ToLinear(valuesUL[i], consts.CameraPlanes.x, consts.CameraPlanes.y);
        valuesBR[i] = ToLinear(valuesBR[i], consts.CameraPlanes.x, consts.CameraPlanes.y);
    }

    float viewspaceZ = valuesUL.y;

    float pixLZ = valuesUL.x;
    float pixTZ = valuesUL.z;
    float pixRZ = valuesBR.z;
    float pixBZ = valuesBR.x;

    vec4 edgesLRTB = XeGTAO_CalculateEdges(viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ);
    imageStore(g_WorkingEdges, pixCoord, vec4(XeGTAO_PackEdges(edgesLRTB)));

    //#define XE_GTAO_GENERATE_NORMALS_INPLACE
#ifdef XE_GTAO_GENERATE_NORMALS_INPLACE
    vec3 CENTER = XeGTAO_ComputeViewspacePosition(normalizedScreenPos,
        viewspaceZ, consts);

    vec3 LEFT = XeGTAO_ComputeViewspacePosition(
        normalizedScreenPos + vec2(-1, 0) * consts.ViewportPixelSize,
        pixLZ, consts);

    vec3 RIGHT = XeGTAO_ComputeViewspacePosition(
        normalizedScreenPos + vec2(1, 0) * consts.ViewportPixelSize,
        pixRZ, consts);

    vec3 TOP = XeGTAO_ComputeViewspacePosition(
        normalizedScreenPos + vec2(0, -1) * consts.ViewportPixelSize,
        pixTZ, consts);

    vec3 BOTTOM = XeGTAO_ComputeViewspacePosition(
        normalizedScreenPos + vec2(0, 1) * consts.ViewportPixelSize,
        pixBZ, consts);

    viewspaceNormal = XeGTAO_CalculateNormal(edgesLRTB, CENTER, LEFT, RIGHT, TOP, BOTTOM);
#else
    viewspaceNormal.z = -viewspaceNormal.z;
#endif

    float viewspaceZScaled =
#ifdef XE_GTAO_FP32_DEPTHS
        viewspaceZ * 0.99999;
#else
        viewspaceZ * 0.99920;
#endif

    vec3 pixCenterPos = XeGTAO_ComputeViewspacePosition(normalizedScreenPos, viewspaceZScaled, consts);
    vec3 viewVec = normalize(-pixCenterPos);

    // constants
#if XE_GTAO_USE_DEFAULT_CONSTANTS != 0
    float effectRadius = consts.EffectRadius * XE_GTAO_DEFAULT_RADIUS_MULTIPLIER;
    float sampleDistributionPower = XE_GTAO_DEFAULT_SAMPLE_DISTRIBUTION_POWER;
    float thinOccluderCompensation = XE_GTAO_DEFAULT_THIN_OCCLUDER_COMPENSATION;
    float falloffRange = XE_GTAO_DEFAULT_FALLOFF_RANGE * effectRadius;
#else
    const float effectRadius = consts.EffectRadius * consts.RadiusMultiplier;
    const float sampleDistributionPower = consts.SampleDistributionPower;
    const float thinOccluderCompensation = consts.ThinOccluderCompensation;
    const float falloffRange = consts.EffectFalloffRange * effectRadius;
#endif

    float falloffFrom = effectRadius * (1.0 - consts.EffectFalloffRange);

    float falloffMul = -1.0 / falloffRange;
    float falloffAdd = falloffFrom / falloffRange + 1.0;

    float visibility = 0.0;
#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
    vec3 bentNormal = viewspaceNormal;
#else
    vec3 bentNormal = vec3(0);
#endif

    float noiseSlice = localNoise.x;
    float noiseSample = localNoise.y;

    vec2 pixelDirRBViewspaceSizeAtCenterZ = viewspaceZ * consts.NDCToViewMul_x_PixelSize;

    float screenspaceRadius = effectRadius / pixelDirRBViewspaceSizeAtCenterZ.x;

    visibility += clamp((10.0 - screenspaceRadius) / 100.0, 0.0, 1.0) * 0.5;

    const float pixelTooCloseThreshold = 1.3;      // if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance
    float minS = pixelTooCloseThreshold / screenspaceRadius;

    for (int slice = 0; slice < int(sliceCount); slice++)
    {
        float sliceK = (float(slice) + noiseSlice) / sliceCount;

        float phi = sliceK * EG_PI;
        float cosPhi = cos(phi);
        float sinPhi = sin(phi);

        vec2 omega = vec2(cosPhi, -sinPhi) * screenspaceRadius;

        vec3 directionVec = vec3(cosPhi, sinPhi, 0.0);
        vec3 orthoDirectionVec = directionVec - dot(directionVec, viewVec) * viewVec;
        vec3 axisVec = normalize(cross(orthoDirectionVec, viewVec));

        vec3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

        float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));

        float projectedLen = length(projectedNormalVec);

        float cosNorm = clamp(dot(projectedNormalVec, viewVec) / projectedLen, 0.0, 1.0);

        float n = signNorm * XeGTAO_FastACos(cosNorm);

        float lowH0 = cos(n + EG_HALF_PI);
        float lowH1 = cos(n - EG_HALF_PI);

        float horizonCos0 = lowH0;
        float horizonCos1 = lowH1;

        for (int step = 0; step < int(stepsPerSlice); step++)
        {
            float stepBaseNoise = float(slice + step * stepsPerSlice) * 0.6180339887;

            float stepNoise = fract(noiseSample + stepBaseNoise);

            float s = (float(step) + stepNoise) / stepsPerSlice;
            s = pow(s, sampleDistributionPower);
            s += minS;

            vec2 sampleOffset = s * omega;

            float sampleLen = length(sampleOffset);

            float mipLevel = clamp(
                log2(sampleLen) - consts.DepthMIPSamplingOffset,
                0.0,
                XE_GTAO_DEPTH_MIP_LEVELS
            );

            sampleOffset = round(sampleOffset) * consts.ViewportPixelSize;

            vec2 sampleScreenPos0 = normalizedScreenPos + sampleOffset;
            vec2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;

            float SZ0 = textureLod(g_Depth, sampleScreenPos0, mipLevel).x;
            float SZ1 = textureLod(g_Depth, sampleScreenPos1, mipLevel).x;
            SZ0 = ToLinear(SZ0, consts.CameraPlanes.x, consts.CameraPlanes.y);
            SZ1 = ToLinear(SZ1, consts.CameraPlanes.x, consts.CameraPlanes.y);

            vec3 samplePos0 = XeGTAO_ComputeViewspacePosition(sampleScreenPos0, SZ0, consts);
            vec3 samplePos1 = XeGTAO_ComputeViewspacePosition(sampleScreenPos1, SZ1, consts);

            vec3 d0 = samplePos0 - pixCenterPos;
            vec3 d1 = samplePos1 - pixCenterPos;

            float dist0 = length(d0);
            float dist1 = length(d1);

            vec3 h0 = d0 / dist0;
            vec3 h1 = d1 / dist1;

#if XE_GTAO_USE_DEFAULT_CONSTANTS != 0 && XE_GTAO_DEFAULT_THIN_OBJECT_HEURISTIC == 0
            float fall0 = clamp(dist0 * falloffMul + falloffAdd, 0.0, 1.0);
            float fall1 = clamp(dist1 * falloffMul + falloffAdd, 0.0, 1.0);
#else
            float falloffBase0 = length(vec3(d0.x, d0.y, d0.z * (1 + thinOccluderCompensation)));
            float falloffBase1 = length(vec3(d1.x, d1.y, d1.z * (1 + thinOccluderCompensation)));
            float fall0 = clamp(falloffBase0 * falloffMul + falloffAdd, 0, 1);
            float fall1 = clamp(falloffBase1 * falloffMul + falloffAdd, 0, 1);
#endif

            float shc0 = dot(h0, viewVec);
            float shc1 = dot(h1, viewVec);

            shc0 = mix(lowH0, shc0, fall0);
            shc1 = mix(lowH1, shc1, fall1);

            // thickness heuristic - see "4.3 Implementation details, Height-field assumption considerations"
#if 0           // (disabled, not used) this should match the paper
            float newhorizonCos0 = max(horizonCos0, shc0);
            float newhorizonCos1 = max(horizonCos1, shc1);
            horizonCos0 = (horizonCos0 > shc0) ? (mix(newhorizonCos0, shc0, thinOccluderCompensation)) : (newhorizonCos0);
            horizonCos1 = (horizonCos1 > shc1) ? (mix(newhorizonCos1, shc1, thinOccluderCompensation)) : (newhorizonCos1);
#elif 0 // (disabled, not used) this is slightly different from the paper but cheaper and provides very similar results
            horizonCos0 = lerp(max(horizonCos0, shc0), shc0, thinOccluderCompensation);
            horizonCos1 = lerp(max(horizonCos1, shc1), shc1, thinOccluderCompensation);
#else   // this is a version where thicknessHeuristic is completely disabled
            horizonCos0 = max(horizonCos0, shc0);
            horizonCos1 = max(horizonCos1, shc1);
#endif
        }

        projectedLen = mix(projectedLen, 1.0, 0.05);

        float h0 = -XeGTAO_FastACos(horizonCos1);
        float h1 = XeGTAO_FastACos(horizonCos0);
#if 1       // we can skip clamping for a tiny little bit more performance
        h0 = n + clamp(h0 - n, -EG_HALF_PI, EG_HALF_PI);
        h1 = n + clamp(h1 - n, -EG_HALF_PI, EG_HALF_PI);
#endif

        float iarc0 =
            (cosNorm + 2.0 * h0 * sin(n) - cos(2.0 * h0 - n)) * 0.25;

        float iarc1 =
            (cosNorm + 2.0 * h1 * sin(n) - cos(2.0 * h1 - n)) * 0.25;

        float localVis = projectedLen * (iarc0 + iarc1);
        visibility += localVis;

#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
        // see "Algorithm 2 Extension that computes bent normals b."
        float t0 = (6 * sin(h0 - n) - sin(3 * h0 - n) + 6 * sin(h1 - n) - sin(3 * h1 - n) + 16 * sin(n) - 3 * (sin(h0 + n) + sin(h1 + n))) / 12;
        float t1 = (-cos(3 * h0 - n) - cos(3 * h1 - n) + 8 * cos(n) - 3 * (cos(h0 + n) + cos(h1 + n))) / 12;
        vec3 localBentNormal = vec3(directionVec.x * t0, directionVec.y * t0, -t1);
        localBentNormal = (XeGTAO_RotFromToMatrix(vec3(0, 0, -1), viewVec) * localBentNormal) * projectedLen;
        bentNormal += localBentNormal;
#endif
    }

    visibility /= sliceCount;
    visibility = pow(visibility, consts.FinalValuePower);
    visibility = max(0.03, visibility);
    visibility = clamp(visibility / XE_GTAO_OCCLUSION_TERM_SCALE, 0, 1);

    imageStore(g_Result, pixCoord, vec4(vec3(visibility), 1));

#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
    // We flipped it at the beginning. Flip back and convert to worlds space
    bentNormal.z = -bentNormal.z;
    bentNormal = normalize(bentNormal);
    bentNormal = inverse(mat3(g_View)) * bentNormal;
    const vec2 encodedNormal = EncodeNormal(bentNormal);
    const uint packed = packHalf2x16(encodedNormal);
    imageStore(g_BentNormalsResult, pixCoord, uvec4(packed));
#endif
}
#endif // #ifdef XE_GTAO_MAIN_PASS

#ifdef XE_GTAO_DENOISE

#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
void XeGTAO_DecodeGatherPartial(vec4 visibilities, uvec4 bentNormals, out AOTermType outDecoded[4])
{
    for (int i = 0; i < 4; i++)
    {
        const vec2 encodedNormal = unpackHalf2x16(bentNormals[i]);
        outDecoded[i].xyz = DecodeNormal(encodedNormal);
        outDecoded[i].w = visibilities[i];
    }
}
#else
void XeGTAO_DecodeGatherPartial(vec4 visibilities, out AOTermType outDecoded[4])
{
    for (int i = 0; i < 4; i++)
    {
        outDecoded[i] = visibilities[i];
    }
}
#endif // #ifdef XE_GTAO_COMPUTE_BENT_NORMALS

void XeGTAO_Output(uvec2 pixCoord, AOTermType outputValue, bool bFinalPass)
{
#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
    float visibility = outputValue.w;
#else
    float visibility = outputValue;
#endif
    visibility *= (bFinalPass ? XE_GTAO_OCCLUSION_TERM_SCALE : 1.0);
    imageStore(g_Result, ivec2(pixCoord), vec4(visibility));

#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
    const vec3 bentNormal = normalize(outputValue.xyz);
    const vec2 encodedNormal = EncodeNormal(bentNormal);
    const uint packed = packHalf2x16(encodedNormal);
    imageStore(g_ResultBentNormals, ivec2(pixCoord), uvec4(packed));
#endif
}

void XeGTAO_Denoise(const ivec2 pixCoordBase, const GTAOConstants consts)
{
    const bool bFinalPass = consts.FinalPass != 0;

    const float blurAmount = bFinalPass ? consts.DenoiseBlurBeta : consts.DenoiseBlurBeta / 5.0;
    const float diagWeight = 0.85 * 0.5;

    AOTermType aoTerm[2];   // pixel pixCoordBase and pixel pixCoordBase + int2( 1, 0 )
    vec4 edgesC_LRTB[2];
    float weightTL[2];
    float weightTR[2];
    float weightBL[2];
    float weightBR[2];

    // gather edge and visibility quads, used later
    const vec2 gatherCenter = vec2(pixCoordBase.x, pixCoordBase.y) * consts.ViewportPixelSize;
    vec4 edgesQ0 = textureGatherOffset(g_Edges, gatherCenter, ivec2(0, 0), 0);
    vec4 edgesQ1 = textureGatherOffset(g_Edges, gatherCenter, ivec2(2, 0), 0);
    vec4 edgesQ2 = textureGatherOffset(g_Edges, gatherCenter, ivec2(1, 2), 0);

    AOTermType visQ0[4];
    AOTermType visQ1[4];
    AOTermType visQ2[4];
    AOTermType visQ3[4];
#ifdef XE_GTAO_COMPUTE_BENT_NORMALS
    XeGTAO_DecodeGatherPartial(textureGatherOffset(g_AO, gatherCenter, ivec2(0, 0), 0), textureGatherOffset(g_BentNormals, gatherCenter, ivec2(0, 0), 0), visQ0);
    XeGTAO_DecodeGatherPartial(textureGatherOffset(g_AO, gatherCenter, ivec2(2, 0), 0), textureGatherOffset(g_BentNormals, gatherCenter, ivec2(2, 0), 0), visQ1);
    XeGTAO_DecodeGatherPartial(textureGatherOffset(g_AO, gatherCenter, ivec2(0, 2), 0), textureGatherOffset(g_BentNormals, gatherCenter, ivec2(0, 2), 0), visQ2);
    XeGTAO_DecodeGatherPartial(textureGatherOffset(g_AO, gatherCenter, ivec2(2, 2), 0), textureGatherOffset(g_BentNormals, gatherCenter, ivec2(2, 2), 0), visQ3);
#else
    XeGTAO_DecodeGatherPartial(textureGatherOffset(g_AO, gatherCenter, ivec2(0, 0), 0), visQ0);
    XeGTAO_DecodeGatherPartial(textureGatherOffset(g_AO, gatherCenter, ivec2(2, 0), 0), visQ1);
    XeGTAO_DecodeGatherPartial(textureGatherOffset(g_AO, gatherCenter, ivec2(0, 2), 0), visQ2);
    XeGTAO_DecodeGatherPartial(textureGatherOffset(g_AO, gatherCenter, ivec2(2, 2), 0), visQ3);
#endif

    for (int side = 0; side < 2; side++)
    {
        const ivec2 pixCoord = ivec2(pixCoordBase.x + side, pixCoordBase.y);

        vec4 edgesL_LRTB = XeGTAO_UnpackEdges((side == 0) ? (edgesQ0.x) : (edgesQ0.y));
        vec4 edgesT_LRTB = XeGTAO_UnpackEdges((side == 0) ? (edgesQ0.z) : (edgesQ1.w));
        vec4 edgesR_LRTB = XeGTAO_UnpackEdges((side == 0) ? (edgesQ1.x) : (edgesQ1.y));
        vec4 edgesB_LRTB = XeGTAO_UnpackEdges((side == 0) ? (edgesQ2.w) : (edgesQ2.z));

        edgesC_LRTB[side] = XeGTAO_UnpackEdges((side == 0) ? (edgesQ0.y) : (edgesQ1.x));

        // Edges aren't perfectly symmetrical: edge detection algorithm does not guarantee that a left edge on the right pixel will match the right edge on the left pixel (although
        // they will match in majority of cases). This line further enforces the symmetricity, creating a slightly sharper blur. Works real nice with TAA.
        edgesC_LRTB[side] *= vec4(edgesL_LRTB.y, edgesR_LRTB.x, edgesT_LRTB.w, edgesB_LRTB.z);

#if 1   // this allows some small amount of AO leaking from neighbours if there are 3 or 4 edges; this reduces both spatial and temporal aliasing
        const float leak_threshold = 2.5;
        const float leak_strength = 0.5;
        float edginess = (clamp(4.0 - leak_threshold - dot(edgesC_LRTB[side], vec4(1)), 0, 1) / (4 - leak_threshold)) * leak_strength;
        edgesC_LRTB[side] = clamp(edgesC_LRTB[side] + edginess, vec4(0), vec4(1));
#endif

        // for diagonals; used by first and second pass
        weightTL[side] = diagWeight * (edgesC_LRTB[side].x * edgesL_LRTB.z + edgesC_LRTB[side].z * edgesT_LRTB.x);
        weightTR[side] = diagWeight * (edgesC_LRTB[side].z * edgesT_LRTB.y + edgesC_LRTB[side].y * edgesR_LRTB.z);
        weightBL[side] = diagWeight * (edgesC_LRTB[side].w * edgesB_LRTB.x + edgesC_LRTB[side].x * edgesL_LRTB.w);
        weightBR[side] = diagWeight * (edgesC_LRTB[side].y * edgesR_LRTB.w + edgesC_LRTB[side].w * edgesB_LRTB.y);

        // first pass
        AOTermType ssaoValue = (side == 0) ? visQ0[1] : visQ1[0];
        AOTermType ssaoValueL = (side == 0) ? visQ0[0] : visQ0[1];
        AOTermType ssaoValueT = (side == 0) ? visQ0[2] : visQ1[3];
        AOTermType ssaoValueR = (side == 0) ? visQ1[0] : visQ1[1];
        AOTermType ssaoValueB = (side == 0) ? visQ2[2] : visQ3[3];
        AOTermType ssaoValueTL = (side == 0) ? visQ0[3] : visQ0[2];
        AOTermType ssaoValueBR = (side == 0) ? visQ3[3] : visQ3[2];
        AOTermType ssaoValueTR = (side == 0) ? visQ1[3] : visQ1[2];
        AOTermType ssaoValueBL = (side == 0) ? visQ2[3] : visQ2[2];

        float sumWeight = blurAmount;
        AOTermType sum = ssaoValue * sumWeight;

        XeGTAO_AddSample(ssaoValueL, edgesC_LRTB[side].x, sum, sumWeight);
        XeGTAO_AddSample(ssaoValueR, edgesC_LRTB[side].y, sum, sumWeight);
        XeGTAO_AddSample(ssaoValueT, edgesC_LRTB[side].z, sum, sumWeight);
        XeGTAO_AddSample(ssaoValueB, edgesC_LRTB[side].w, sum, sumWeight);

        XeGTAO_AddSample(ssaoValueTL, weightTL[side], sum, sumWeight);
        XeGTAO_AddSample(ssaoValueTR, weightTR[side], sum, sumWeight);
        XeGTAO_AddSample(ssaoValueBL, weightBL[side], sum, sumWeight);
        XeGTAO_AddSample(ssaoValueBR, weightBR[side], sum, sumWeight);

        aoTerm[side] = sum / sumWeight;

        XeGTAO_Output(pixCoord, aoTerm[side], bFinalPass);

#ifdef XE_GTAO_SHOW_BENT_NORMALS
        if (bFinalPass)
        {
            g_outputDbgImage[pixCoord] = float4(DisplayNormalSRGB(aoTerm[side].xyz /** aoTerm[side].www*/), 1);
        }
#endif
    }
}
#endif // #ifdef XE_GTAO_DENOISE

#endif
