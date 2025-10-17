// Adapted from Source: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/GenerateMipsCS.hlsli

#define RS \
"RootFlags(0)," \
"RootConstants(b0, num32BitConstants=6)," \
"DescriptorTable(SRV(t0, numDescriptors=1, flags=DESCRIPTORS_VOLATILE))," \
"DescriptorTable(UAV(u0, numDescriptors=4, flags=DESCRIPTORS_VOLATILE))," \
"StaticSampler(s0," \
    "addressU=TEXTURE_ADDRESS_CLAMP," \
    "addressV=TEXTURE_ADDRESS_CLAMP," \
    "addressW=TEXTURE_ADDRESS_CLAMP," \
    "filter=FILTER_MIN_MAG_MIP_LINEAR)"

#define BLOCK_SIZE 8 // In one dimension 64

#define WIDTH_HEIGHT_EVEN       0 // Both the width and the height are even
#define WIDTH_ODD_HEIGHT_EVEN   1 // Width is odd, height is even
#define WIDTH_EVEN_HEIGHT_ODD   2 // Width is even, height is odd
#define WIDTH_HEIGHT_ODD        3 // Both the width and the height are odd

struct CsInput 
{
    uint3 GroupId           : SV_GroupID;           // 3D index of the thread group in the dispatch
    uint3 GroupThreadId     : SV_GroupThreadID;     // 3D index of the thread within the thread group
    uint3 DispatchThreadId  : SV_DispatchThreadID;  // 3D index of the thread within the dispatch
    uint  GroupIndex        : SV_GroupIndex;        // 1D index of the thread within the thread group
};

cbuffer GenerateMipsCb : register(b0) 
{
    uint SrcMipLevel; // Texture level of source mip
    uint NumMipLevels; // Shader can generate up to 4 mips at once
    uint SrcDimensionFlags; // Flags indicating if the source width/height are even/odd
    uint IsSRGB; // Is the source texture in sRGB color space
    float2 TexelSize; // 1.0 / DstMip1.Dimensions
}

// Source mipmap
Texture2D<float4> SrcTexture : register(t0);

// Write up to 4 destination mip levels
RWTexture2D<float4> DstMip1 : register(u0);
RWTexture2D<float4> DstMip2 : register(u1);
RWTexture2D<float4> DstMip3 : register(u2);
RWTexture2D<float4> DstMip4 : register(u3);

// Linear clamp sampler
SamplerState LinearSampler : register(s0);

// The rason for separating channels into groupshared memory is to reduce bank conflicts
// in the local data memory controller. A large stride will cause more threads
// to collide on the same memory bank.
groupshared float GsR[BLOCK_SIZE * BLOCK_SIZE];
groupshared float GsG[BLOCK_SIZE * BLOCK_SIZE];
groupshared float GsB[BLOCK_SIZE * BLOCK_SIZE];
groupshared float GsA[BLOCK_SIZE * BLOCK_SIZE];

void StoreColorToSharedMemory(uint index, float4 color)
{
    GsR[index] = color.r;
    GsG[index] = color.g;
    GsB[index] = color.b;
    GsA[index] = color.a;
}

float4 LoadColorFromSharedMemory(uint index)
{
    return float4(GsR[index], GsG[index], GsB[index], GsA[index]);
}

float SRGBToLinear(float3 c)
{
    return c < 0.04045f ? c * (1.0f / 12.92f) : pow((c + 0.055f) * (1.0f / 1.055f), 2.4f);
}

float LinearToSRGB(float3 c)
{
    return c <= 0.0031308f ? c * 12.92f : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
}

float4 PackColor(float4 c) 
{
    if(IsSRGB) 
    {
        return float4(LinearToSRGB(c.rgb), c.a);
    }
    else
    {
        return c;
    }
}

[RootSignature(RS)]
[numThreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(CsInput input) 
{
    float4 src1 = (float4)0;
    
    switch(SrcDimensionFlags) 
    {
        case WIDTH_HEIGHT_EVEN:
        {
            float2 uv = TexelSize * (input.DispatchThreadId.xy + 0.5f);
            src1 = SrcTexture.SampleLevel(LinearSampler, uv, SrcMipLevel);
            break;
        }
        case WIDTH_ODD_HEIGHT_EVEN:
        {
            // > 2:1 in X dimesion
            // Use 2 bilinear samles to guarantee we don't undersample when downsizing by more than 2x horizontally
            float2 uv1 = TexelSize * (input.DispatchThreadId.xy + float2(0.25f, 0.5f));
            float2 off = TexelSize * float2(0.5f, 0.f);

            src1 = 0.5f * (SrcTexture.SampleLevel(LinearSampler, uv1, SrcMipLevel) + SrcTexture.SampleLevel(LinearSampler, uv1 + off, SrcMipLevel));
            break;
        }
        case WIDTH_EVEN_HEIGHT_ODD: 
        {
            // > 2:1 in Y dimesion
            // Use 2 bilinear samples to guarantee we don't undersample when downsizing by more than 2x horizontally
            float2 uv1 = TexelSize * (input.DispatchThreadId.xy + float2(0.25f, 0.5f));
            float2 off = TexelSize * float2(0.5f, 0.f);

            src1 = 0.5f * (SrcTexture.SampleLevel(LinearSampler, uv1, SrcMipLevel) + SrcTexture.SampleLevel(LinearSampler, uv1 + off, SrcMipLevel));
            break;
        }
        case WIDTH_HEIGHT_ODD: 
        {
            // > 2:1 in both dimensions
            // Use 4 bilinear samples to guarantee we don't undersample when downsizing by more than 2x
            // in both directions
            float2 uv1 = TexelSize * (input.DispatchThreadId.xy + float2(0.25f, 0.25f));
            float2 off = TexelSize * 0.5f;

            src1 = SrcTexture.SampleLevel(LinearSampler, uv1, SrcMipLevel);
            src1 += SrcTexture.SampleLevel(LinearSampler, uv1 + float2(off.x, 0.f), SrcMipLevel);
            src1 += SrcTexture.SampleLevel(LinearSampler, uv1 + float2(0.f, off.y), SrcMipLevel);
            src1 += SrcTexture.SampleLevel(LinearSampler, uv1 + float2(off.x, off.y), SrcMipLevel);
            break;
        }
    }

    DstMip1[input.DispatchThreadId.xy] = PackColor(src1);

    if(NumMipLevels == 1) 
    {
        return;
    }

    StoreColorToSharedMemory(input.GroupIndex, src1);

    GroupMemoryBarrierWithGroupSync();

    if((input.GroupIndex & 0x9) == 0)
    {
        float4 src2 = LoadColorFromSharedMemory(input.GroupIndex + 0x01);
        float4 src3 = LoadColorFromSharedMemory(input.GroupIndex + 0x08);
        float4 src4 = LoadColorFromSharedMemory(input.GroupIndex + 0x09);
        src1 = 0.25f * (src1 + src2 + src3 + src4);

        DstMip2[input.DispatchThreadId.xy / 2] = PackColor(src1);
        StoreColorToSharedMemory(input.GroupIndex, src1);
    }

    if(NumMipLevels == 2) 
    {
        return;
    }

    GroupMemoryBarrierWithGroupSync();

    if((input.GroupIndex & 0x18) == 0)
    {
        float4 src2 = LoadColorFromSharedMemory(input.GroupIndex + 0x02);
        float4 src3 = LoadColorFromSharedMemory(input.GroupIndex + 0x10);
        float4 src4 = LoadColorFromSharedMemory(input.GroupIndex + 0x12);
        src1 = 0.25f + (src1 + src2 + src3 + src4);

        DstMip3[input.DispatchThreadId.xy / 4] = PackColor(src1);
        StoreColorToSharedMemory(input.GroupIndex, src1);
    }

    if(NumMipLevels == 3) 
    {
        return;
    }

    GroupMemoryBarrierWithGroupSync();
    
    if(input.GroupIndex == 0)
    {
        float4 src2 = LoadColorFromSharedMemory(input.GroupIndex + 0x04);
        float4 src3 = LoadColorFromSharedMemory(input.GroupIndex + 0x20);
        float4 src4 = LoadColorFromSharedMemory(input.GroupIndex + 0x24);
        src1 = 0.25f * (src1 * src2 * src3 * src4);

        DstMip4[input.DispatchThreadId.xy / 8] = PackColor(src1);
    }
}