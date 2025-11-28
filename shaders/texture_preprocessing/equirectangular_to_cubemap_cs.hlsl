#define RS \
"RootFlags(0)," \
"RootConstants(b0, num32BitConstants=4), " \
"DescriptorTable(SRV(t0, numDescriptors=1, flags=DESCRIPTORS_VOLATILE))," \
"DescriptorTable(UAV(u0, numDescriptors=5, flags=DESCRIPTORS_VOLATILE))," \
"StaticSampler(s0," \
    "addressU=TEXTURE_ADDRESS_WRAP," \
    "addressV=TEXTURE_ADDRESS_WRAP," \
    "addressW=TEXTURE_ADDRESS_WRAP," \
    "filter=FILTER_MIN_MAG_MIP_LINEAR)"

#include "../common/math.hlsli"
#define BLOCK_SIZE 16

#include "../common/cs.hlsli"

cbuffer EquirectangularToCubemapCb : register(b0) 
{
    uint CubemapSize;   // Size fo cubemap face in pixels
    uint FirstMipLevel; // First mip level to process
    uint NumMipLevels;  // Number of mip levels to process
    bool IsSRGB;        // Is the source texture in sRGB color space
};

// Source equirectangular texture as an panoramic image
// It is assumed that the src texture has a full mipmap chain
Texture2D<float4> SrcTexture : register(t0);

// Destination cubemap texture as a mip slice in the cubemap texture (texture array with 6 elements)
RWTexture2DArray<float4> DstMip1 : register(u0);
RWTexture2DArray<float4> DstMip2 : register(u1);
RWTexture2DArray<float4> DstMip3 : register(u2);
RWTexture2DArray<float4> DstMip4 : register(u3);
RWTexture2DArray<float4> DstMip5 : register(u4);

// Linear repeat sampler
SamplerState LinearRepeatSampler : register(s0);

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

float SRGBToLinear(float3 x) 
{
    return x.r < 0.04045f && x.g < 0.04045f && x.b < 0.04045f ? x / 12.92f : pow((x + 0.055f) / 1.055f, 2.4f);
}

float3 LinearToSRGB(float3 x) 
{
    return x.r < 0.0031308f && x.g < 0.0031308f && x.b < 0.0031308f ? 12.92f * x : 1.055f * pow(abs(x), 1.f / 2.4f) - 0.055f;
}

float PackColor(float4 x) 
{
    if(IsSRGB) 
    {
        return float4(LinearToSRGB(x.rgb), x.a);
    }
    else 
    {
        return x;
    }
}

[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(CsInput input) 
{
    // Cubemap texture coords.
	uint3 texCoord = input.DispatchThreadId;

	// First check if the thread is in the cubemap dimensions.
	if (texCoord.x >= CubemapSize || texCoord.y >= CubemapSize) return;

	// Map the UV coords of the cubemap face to a direction
	// [(0, 0), (1, 1)] => [(-0.5, -0.5), (0.5, 0.5)]
	float3 dir = float3(texCoord.xy / float(CubemapSize) - 0.5f, 0.5f);

	// Rotate to cubemap face
	dir = normalize(mul(RotateUV[texCoord.z], dir));
	dir.z *= -1.f;


	// Convert the world space direction into U,V texture coordinates in the panoramic texture.
	// Source: http://gl.ict.usc.edu/Data/HighResProbes/
	float2 panoUV = float2(atan2(-dir.x, -dir.z), acos(dir.y)) * INV_ATAN;

	DstMip1[texCoord] = PackColor(SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMipLevel));

	// Only perform on threads that are a multiple of 2.
	if (NumMipLevels > 1 && (input.GroupIndex & 0x11) == 0)
	{
		DstMip2[uint3(texCoord.xy / 2, texCoord.z)] = PackColor(SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMipLevel + 1));
	}

	// Only perform on threads that are a multiple of 4.
	if (NumMipLevels > 2 && (input.GroupIndex & 0x33) == 0)
	{
		DstMip3[uint3(texCoord.xy / 4, texCoord.z)] = PackColor(SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMipLevel + 2));
	}

	// Only perform on threads that are a multiple of 8.
	if (NumMipLevels > 3 && (input.GroupIndex & 0x77) == 0)
	{
		DstMip4[uint3(texCoord.xy / 8, texCoord.z)] = PackColor(SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMipLevel + 3));
	}

	// Only perform on threads that are a multiple of 16.
	// This should only be thread 0 in this group.
	if (NumMipLevels > 4 && (input.GroupIndex & 0xFF) == 0)
	{
		DstMip5[uint3(texCoord.xy / 16, texCoord.z)] = PackColor(SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMipLevel + 4));
	}
}