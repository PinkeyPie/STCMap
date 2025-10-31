#define RS \
"RootFlags(0)," \
"RootConstants(b0, num32BitConstants=2)," \
"DescriptorTable(SRV(t0, numDescriptors=1, flags=DESCRIPTORS_VOLATILE))," \
"DescriptorTable(UAV(u0, numDescriptors=1, flags=DESCRIPTORS_VOLATILE))," \
"StaticSampler(s0," \
    "addressU=TEXTURE_ADDRESS_WRAP," \
    "addressV=TEXTURE_ADDRESS_WRAP," \
    "addressW=TEXTURE_ADDRESS_WRAP," \
    "filter=FILTER_MIN_MAG_MIP_LINEAR)"

#include "../common/math.hlsli"
#define BLOCK_SIZE 16

#include "../common/cs.hlsli"

cbuffer CubemmapToIrradianceCb : register(b0) 
{
    uint IrradianceMapSize; // Size fo cubemap face in pixels
    float UvzScale;
}

TextureCube<float4> SrcTexture : register(t0);

RWTexture2DArray<float4> DstTexture : register(u0);

SamplerState LinearSampler : register(s0);

// Transfrom from dispatch Id to cubemap face direction
static const float3x3 RotateUV[6] = {
    // +x
    float3x3( 0,  0,  1,
              0, -1,  0,
             -1,  0,  0),
    // -x
    float3x3( 0,  0, -1,
              0, -1,  0,
              1,  0,  0),
    // +y
    float3x3( 1,  0,  0,
              0,  0,  1,
              0,  1,  0),
    // -y
    float3x3( 1,  0,  0,
              0,  0, -1,
              0, -1,  0),
    // +z
    float3x3( 1,  0,  0,
              0, -1,  0,
              0,  0,  1),
    // -z
    float3x3(-1,  0,  0,
              0, -1,  0,
              0,  0, -1)
};

[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(CsInput input) 
{
    uint3 texCoord = input.DispatchThreadId;

    if(texCoord.x >= IrradianceMapSize || texCoord.y >= IrradianceMapSize) 
    {
        return;
    }

    float3 dir = float3(texCoord.xy / float(IrradianceMapSize) - 0.5f, 0.5f);
    dir = normalize(mul(RotateUV[texCoord.z], dir));

    float3 up = float3(0.f, 1.f, 0.f);
    float3 right = normalize(cross(up, dir));
    up = normalize(cross(dir, right));

    float3 irradiance = float3(0.f, 0.f, 0.f);

    uint srcWidth, srcHeight, numMips;
    SrcTexture.GetDimensions(0, srcWidth, srcHeight, numMips);

    float sampleLevel = log2((float)srcWidth / (float)IrradianceMapSize);

    const float SAMPLE_DELTA = 0.025f;
    float nrSamples = 0.f;
    for(float phi = 0.f; phi < 2.f * PI; phi += SAMPLE_DELTA) 
    {
        for(float theta = 0.f; theta < 0.5f * PI; theta += SAMPLE_DELTA) 
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample;
            tangentSample.x = sin(theta) * cos(phi);
            tangentSample.y = sin(theta) * sin(phi);
            tangentSample.z = cos(theta);

            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * dir;

            irradiance += SrcTexture.SampleLevel(LinearSampler, sampleVec, sampleLevel).xyz * cos(theta) * sin(theta);
            nrSamples++;
        }
    }

    irradiance = PI * irradiance * (1.f / nrSamples);

    DstTexture[uint3(texCoord.xy, texCoord.z)] = float4(irradiance, 1.f);
}