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

[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(CsInput input) 
{
    // Cubemap texture coords.
	uint3 texCoord = input.DispatchThreadId;

	// First check if the thread is in the cubemap dimensions.
	if (texCoord.x >= IrradianceMapSize || texCoord.y >= IrradianceMapSize) return;

	// Map the UV coords of the cubemap face to a direction.
	// [(0, 0), (1, 1)] => [(-0.5, -0.5), (0.5, 0.5)]
	float3 dir = float3(texCoord.xy / float(IrradianceMapSize) - 0.5f, 0.5f);
	dir = normalize(mul(RotateUV[texCoord.z], dir));

	float3 up = float3(0.f, 1.f, 0.f);
	float3 right = cross(up, dir);
	up = cross(dir, right);

	float3 irradiance = float3(0.f, 0.f, 0.f);


	uint srcWidth, srcHeight, numMipLevels;
	SrcTexture.GetDimensions(0, srcWidth, srcHeight, numMipLevels);

	float sampleMipLevel = log2((float)srcWidth / (float)IrradianceMapSize);

	const float sampleDelta = 0.025f;
	float nrSamples = 0.f;
	for (float phi = 0.f; phi < 2.f * PI; phi += sampleDelta)
	{
		for (float theta = 0.f; theta < 0.5f * PI; theta += sampleDelta)
		{
			float sinTheta, cosTheta;
			float sinPhi, cosPhi;
			sincos(theta, sinTheta, cosTheta);
			sincos(phi, sinPhi, cosPhi);

			// Spherical to cartesian (in tangent space).
			float3 tangentSample = float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
			// Tangent space to world.
			float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * dir;

			sampleVec.z *= UvzScale;

			float4 color = SrcTexture.SampleLevel(LinearRepeatSampler, sampleVec, sampleMipLevel);
			irradiance += color.xyz * cosTheta * sinTheta;
			nrSamples++;
		}
	}

	irradiance = PI * irradiance * (1.f / float(nrSamples));

	DstTexture[texCoord] = float4(irradiance, 1.f);
}