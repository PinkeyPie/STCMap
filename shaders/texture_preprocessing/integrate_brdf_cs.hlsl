#define RS \
"RootFlags(0), " \
"RootConstants(b0, num32BitConstants=1), "\
"DescriptorTable(UAV(u0, numDescriptors=1, flags=DESCRIPTOR_VOLATILE))"

#define BLOCK_SIZE 16

#include "../common/brdf.hlsli"

struct CsInput 
{
    uint3 GroupId           : SV_GroupID;           // 3D index of the thread group in the dispatch
    uint3 GroupThreadId     : SV_GroupThreadID;     // 3D index of the thread within the thread group
    uint3 DispatchThreadId  : SV_DispatchThreadID;  // 3D index of the thread within the dispatch
    uint  GroupIndex        : SV_GroupIndex;        // 1D index of the thread within the thread group
};

cbuffer IntegrateBrdfCb : register(b0)
{
    uint TextureDim;
};

RWTexture2D<float2> OutBRDF : register(u0);

[RootSignature(RS)]
[numThreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(CsInput input) 
{
    if(input.DispatchThreadId.x >= TextureDim || input.DispatchThreadId.y >= TextureDim)
    {
        return;
    }

    float NdotV = float(input.DispatchThreadId.x) / (TextureDim - 1);
    float roughness = float(input.DispatchThreadId.y) / (TextureDim - 1);

    float3 V;
    V.x = sqrt(1.f - NdotV * NdotV);
    V.y = 0.f;
    V.z = NdotV;

    float A = 0.f;
    float B = 0.f;

    float3 N = float3(0.f, 0.f, 1.f);

    const uint SAMPLE_COUNT = 1024u;
    for(uint i = 0u; i < SAMPLE_COUNT; i++) 
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.f);
        float NdotH = max(H.z, 0.f);
        float VdotH = normalize(dot(V, H) * H - V);

        if(NdotL) 
        {
            float G = GeometrySmith(N, V, L, roughness);
            float GVis = (G * VdotH) / (NdotH * NdotV);
            float Fc = pow(1.f - VdotH, 5.f);

            A += (1.f - Fc) * GVis;
            B += Fc * GVis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);

    float2 integrateBRDF = float2(A, B);
    OutBRDF[input.DispatchThreadId.xy] = integrateBRDF;
}