#define RS \
"RootFlags(0), " \
"RootConstants(b0, num32BitConstants=4), " \
"DescriptorTable(SRV(t0, numDescriptors=1, flags=DESCRIPTORS_VOLATILE)), " \
"DescriptorTable(UAV(u0, numDescriptors=5, flags=DESCRIPTORS_VOLATILE)), " \
"StaticSampler(s0," \
    "addressU=TEXTURE_ADDRESS_WRAP," \
    "addressV=TEXTURE_ADDRESS_WRAP," \
    "addressW=TEXTURE_ADDRESS_WRAP," \
    "filter=FILTER_MIN_MAG_MIP_LINEAR)"

#define BLOCK_SIZE 16

#include "../common/brdf.hlsli"
#include "../common/math.hlsli"
#include "../common/random.hlsli"
#include "../common/cs.hlsli"

cbuffer PrefilterEnvironmentCb : register(b0)
{
    uint CubemapSize;           // Size of cubemap face in pixels at the current mipmal level
    uint FirstMip;              // The first mip level to generate
    uint NumMipLevels;          // The number of mips to generate
    uint TotalNumMipLevels;
};

TextureCube<float4> SrcTexture : register(t0);

RWTexture2DArray<float4> DstMip1 : register(u0);
RWTexture2DArray<float4> DstMip2 : register(u1);
RWTexture2DArray<float4> DstMip3 : register(u2);
RWTexture2DArray<float4> DstMip4 : register(u3);
RWTexture2DArray<float4> DstMip5 : register(u4);

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

static float4 Filter(uint mip, float3 N, float3 V) 
{
    float roughness = float(mip) / (TotalNumMipLevels - 1);

	const uint SAMPLE_COUNT = 1024u;
	float totalWeight = 0.f;
	float3 prefilteredColor = float3(0.f, 0.f, 0.f);


	uint width, height, numMipLevels;
	SrcTexture.GetDimensions(0, width, height, numMipLevels);

	for (uint i = 0; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H = ImportanceSampleGGX(Xi, N, roughness).xyz;
		float3 L = normalize(2.f * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.f);
		float NdotH = max(dot(N, H), 0.f);
		float HdotV = max(dot(H, V), 0.f);
		if (NdotL > 0.f)
		{
			float D = DistributionGGX(NdotH, roughness);
			float pdf = (D * NdotH / (4.f * HdotV)) + 0.0001f;

			uint resolution = width; // We expect quadratic faces, so width == height.
			float saTexel = 4.f * PI / (6.f * width * height);
			float saSample = 1.f / (SAMPLE_COUNT * pdf + 0.00001f);

			float sampleMipLevel = (roughness == 0.f) ? 0.f : 0.5f * log2(saSample / saTexel);

			prefilteredColor += SrcTexture.SampleLevel(LinearRepeatSampler, L, sampleMipLevel).xyz * NdotL;
			totalWeight += NdotL;
		}
	}
	prefilteredColor = prefilteredColor / totalWeight;
	return float4(prefilteredColor, 1.f);
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
	float3 N = float3(texCoord.xy / float(CubemapSize) - 0.5f, 0.5f);
	N = normalize(mul(RotateUV[texCoord.z], N));

	float3 R = N;
	float3 V = R;

	DstMip1[texCoord] = Filter(FirstMip, N, V);

	if (NumMipLevels > 1 && (input.GroupIndex & 0x11) == 0)
	{
		DstMip2[uint3(texCoord.xy / 2, texCoord.z)] = Filter(FirstMip + 1, N, V);
	}

	if (NumMipLevels > 2 && (input.GroupIndex & 0x33) == 0)
	{
		DstMip3[uint3(texCoord.xy / 4, texCoord.z)] = Filter(FirstMip + 2, N, V);
	}

	if (NumMipLevels > 3 && (input.GroupIndex & 0x77) == 0)
	{
		DstMip4[uint3(texCoord.xy / 8, texCoord.z)] = Filter(FirstMip + 3, N, V);
	}

	if (NumMipLevels > 4 && (input.GroupIndex & 0xFF) == 0)
	{
		DstMip5[uint3(texCoord.xy / 16, texCoord.z)] = Filter(FirstMip + 4, N, V);
	}
}