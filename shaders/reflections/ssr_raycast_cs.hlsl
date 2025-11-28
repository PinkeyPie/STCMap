#include "../rs/ssr_rs.hlsli"
#include "../common/cs.hlsli"
#include "../common/camera.hlsli"
#include "../common/normal.hlsli"
#include "../common/brdf.hlsli"
#include "../common/random.hlsli"

ConstantBuffer<SsrRaycastCb> Raycast	: register(b0);
ConstantBuffer<CameraCb> Camera	        : register(b1);

Texture2D<float> DepthBuffer		: register(t0);
Texture2D<float> LinearDepthBuffer	: register(t1);
Texture2D<float2> WorldNormals		: register(t2);
Texture2D<float4> Reflectance		: register(t3);
Texture2D<float2> Noise     		: register(t4);


RWTexture2D<float4> Output			: register(u0);

SamplerState LinearSampler			: register(s0);
SamplerState PointSampler			: register(s1);

static float DistanceSquared(float2 a, float2 b) 
{
    a -= b;
    return dot(a, a);
}

static void Swap(inout float a, inout float b)
{
    float t = a;
    a = b;
    b = t;
}

static bool IntersectsDepthBuffer(float sceneZMax, float rayZMin, float rayZMax)
{
    // Increase thickness along distance. 
    float thickness = max(sceneZMax * 0.3f, 1.f);

    // Effectively remove line/tiny artifacts, mostly caused by Zbuffers precision.
    float depthScale = min(1.f, sceneZMax / 100.f);
    sceneZMax += lerp(0.05f, 0.f, depthScale);

    return (rayZMin >= sceneZMax) && (rayZMax - thickness <= sceneZMax);
}

static bool TraceScreenSpaceRay(float3 rayOrigin, float3 rayDirection, float jitter, float roughness,
    out float2 hitPixel)
{
    hitPixel = float2(-1.f, -1.f);

    const float cameraNearPlane = -Camera.ProjectionParams.x; // Now negative.
    float rayLength = ((rayOrigin.z + rayDirection.z * Raycast.MaxDistance) > cameraNearPlane)
        ? (cameraNearPlane - rayOrigin.z) / rayDirection.z
        : Raycast.MaxDistance;

    float3 rayEnd = rayOrigin + rayDirection * rayLength;

    // Project into screen space.
    float4 H0 = mul(Camera.Proj, float4(rayOrigin, 1.f));
    float4 H1 = mul(Camera.Proj, float4(rayEnd, 1.f));
    float k0 = 1.f / H0.w;
    float k1 = 1.f / H1.w;
    float3 Q0 = rayOrigin * k0;
    float3 Q1 = rayEnd * k1;

    Q0.z *= -1.f;
    Q1.z *= -1.f;

    // Screen space endpoints.
    float2 P0 = H0.xy * k0;
    float2 P1 = H1.xy * k1;
    P0 = P0 * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    P1 = P1 * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
    P0.xy *= Raycast.Dimensions;
    P1.xy *= Raycast.Dimensions;

    // Avoid degenerate lines.
    P1 += (DistanceSquared(P0, P1) < 0.0001f) ? 0.01f : 0.f;

    float2 screenDelta = P1 - P0;

    bool permute = false;
    if (abs(screenDelta.x) < abs(screenDelta.y))
    {
        permute = true;
        screenDelta = screenDelta.yx;
        P0 = P0.yx;
        P1 = P1.yx;
    }

    float stepSign = sign(screenDelta.x);
    float invdx = stepSign / screenDelta.x;

    // Derivatives of Q and k.
    float3 dQ = (Q1 - Q0) * invdx;
    float  dk = (k1 - k0) * invdx;
    float2 dP = float2(stepSign, screenDelta.y * invdx);

    float zMin = min(-rayEnd.z, -rayOrigin.z);
    float zMax = max(-rayEnd.z, -rayOrigin.z);


    // Stride based on roughness. Matte materials will recieve higher stride.
    float alphaRoughness = roughness * roughness;
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;

    float strideScale = 1.f - min(1.f, -rayOrigin.z / Raycast.StrideCutoff);
    float strideRoughnessScale = lerp(Raycast.MinStride, Raycast.MaxStride, min(alphaRoughnessSq, 1.f)); // Climb exponentially at extreme conditions.
    float pixelStride = 1.f + strideScale * strideRoughnessScale;

    // Scale derivatives by stride.
    dP *= pixelStride; dQ *= pixelStride; dk *= pixelStride;
    P0 += dP * jitter; Q0 += dQ * jitter; k0 += dk * jitter;

    // Start values and derivatives packed together -> only one operation needed to increase.
    float4 PQk = float4(P0, Q0.z, k0);
    float4 dPQk = float4(dP, dQ.z, dk);


    float level = 0.f;

    float prevZMaxEstimate = -rayOrigin.z;
    float rayZMin = -rayOrigin.z;
    float rayZMax = -rayOrigin.z;
    float sceneZMax = rayZMax + 100000.0f;
    
    uint stepCount = 0;
    float end = P1.x * stepSign;

    hitPixel = float2(0.f, 0.f);

    while (((PQk.x * stepSign) <= end) &&
        (stepCount < Raycast.NumSteps) &&
        (!IntersectsDepthBuffer(sceneZMax, rayZMin, rayZMax)) &&
        (sceneZMax != 0.f))
    {
        if (any(or(hitPixel < 0.f, hitPixel > 1.f)))
        {
            return false;
        }

        rayZMin = prevZMaxEstimate;

        // Compute the value at 1/2 step into the future.
        rayZMax = (dPQk.z * 0.5f + PQk.z) / (dPQk.w * 0.5f + PQk.w);
        rayZMax = clamp(rayZMax, zMin, zMax);

        prevZMaxEstimate = rayZMax;

        if (rayZMin > rayZMax) { Swap(rayZMin, rayZMax); }

        const float hzbBias = 0.05f;
        const float hzbMinStep = 0.005f;

        // A simple HZB approach based on roughness.
        level += max(hzbBias * roughness, hzbMinStep);
        level = min(level, 6.f);

        hitPixel = permute ? PQk.yx : PQk.xy;
        hitPixel *= Raycast.InvDimensions;

        sceneZMax = LinearDepthBuffer.SampleLevel(LinearSampler, hitPixel, level); // Already in world units.


        PQk += dPQk;
        ++stepCount;
    }

    return IntersectsDepthBuffer(sceneZMax, rayZMin, rayZMax);
}

[numthreads(SSR_BLOCK_SIZE, SSR_BLOCK_SIZE, 1)]
[RootSignature(SSR_RAYCAST_RS)]
void main(CsInput cin)
{
    float2 uv = (cin.DispatchThreadId.xy + 0.5f) * Raycast.InvDimensions;

    const float depth = DepthBuffer.SampleLevel(PointSampler, uv, 0);
    if (depth == 1.f)
    {
        Output[cin.DispatchThreadId.xy] = float4(0.f, 0.f, -1.f, 0.f);
        return;
    }

    const float3 normal = UnpackNormal(WorldNormals.SampleLevel(LinearSampler, uv, 0));
    const float3 viewNormal = mul(Camera.View, float4(normal, 0.f)).xyz;
    const float roughness = clamp(Reflectance.SampleLevel(LinearSampler, uv, 0).a, 0.03f, 0.97f);
    
    const float3 viewPos = RestoreViewSpacePosition(uv, depth, Camera.InvProj);
    const float3 viewDir = normalize(viewPos);

    float2 h = Halton23(Raycast.FrameIndex & 1023);
    uint3 noiseDims;
    Noise.GetDimensions(0, noiseDims.x, noiseDims.y, noiseDims.z);
    float2 Xi = Noise.SampleLevel(LinearSampler, (uv + h) * Raycast.Dimensions / float2(noiseDims.xy), 0);
    Xi.y = lerp(Xi.y, 0.f, SSR_GGX_IMPORTANCE_SAMPLE_BIAS);

    float4 H = ImportanceSampleGGX(Xi, viewNormal, roughness);
    float3 reflDir = reflect(viewDir, H.xyz);

    float jitter = InterleavedGradientNoise(cin.DispatchThreadId.xy, Raycast.FrameIndex);

    float2 hitPixel;
    bool hit = TraceScreenSpaceRay(viewPos + reflDir * 0.02f, reflDir, jitter, roughness, hitPixel);

    float hitDepth = DepthBuffer.SampleLevel(PointSampler, hitPixel, 0);
    float hitMul = hit ? 1.f : -1.f; // We pack the info whether we hit or not in the sign of the depth.
    Output[cin.DispatchThreadId.xy] = float4(hitPixel, hitDepth * hitMul, H.w);
}