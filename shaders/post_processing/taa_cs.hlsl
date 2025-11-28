#include "../rs/post_processing_rs.hlsli"
#include "../common/cs.hlsli"
#include "../common/camera.hlsli"
#include "../common/math.hlsli"

ConstantBuffer<TaaCb> Taa			: register(b0);

RWTexture2D<float4> Output			: register(u0);
Texture2D<float4> CurrentFrame		: register(t0);
Texture2D<float4> PrevFrame			: register(t1);
Texture2D<float2> Motion			: register(t2);
Texture2D<float> DepthBuffer		: register(t3);

SamplerState LinearSampler			: register(s0);


#define TILE_BORDER 1
#define TILE_SIZE (POST_PROCESSING_BLOCK_SIZE + TILE_BORDER * 2)

#define HDR_CORRECTION

groupshared uint TileRedGreen[TILE_SIZE * TILE_SIZE];
groupshared uint TileBlueDepth[TILE_SIZE * TILE_SIZE];


static float3 Tonemap(float3 x)
{
	return x / (x + 1.f); // Reinhard tonemap
}

static float3 InverseTonemap(float3 x)
{
	return x / (1.f - x);
}

[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(TAA_RS)]
void main(CsInput cin)
{
	int2 texCoord = cin.DispatchThreadId.xy;
	float2 invDimensions = 1.f / Taa.Dimensions;
	float2 uv = (float2(texCoord) + float2(0.5f, 0.5f)) * invDimensions;

	const int2 upperLeft = cin.GroupId.xy * POST_PROCESSING_BLOCK_SIZE - TILE_BORDER;

	for (uint t = cin.GroupIndex; t < TILE_SIZE * TILE_SIZE; t += POST_PROCESSING_BLOCK_SIZE * POST_PROCESSING_BLOCK_SIZE)
	{
		const uint2 pixel = upperLeft + Unflatten2D(t, TILE_SIZE);
		const float depth = DepthBuffer[pixel];
		const float3 color = CurrentFrame[pixel].rgb;
		TileRedGreen[t] = f32tof16(color.r) | (f32tof16(color.g) << 16);
		TileBlueDepth[t] = f32tof16(color.b) | (f32tof16(depth) << 16);
	}
	GroupMemoryBarrierWithGroupSync();


	float3 neighborhoodMin = 100000;
	float3 neighborhoodMax = -100000;
	float3 current;
	float bestDepth = 1;

	// Search for best velocity and compute color clamping range in 3x3 neighborhood.
	int2 bestOffset = 0;
	for (int x = -1; x <= 1; ++x)
	{
		for (int y = -1; y <= 1; ++y)
		{
			const int2 offset = int2(x, y);
			const uint idx = Flatten2D(cin.GroupThreadId.xy + TILE_BORDER + offset, TILE_SIZE);
			const uint redGreen = TileRedGreen[idx];
			const uint blueDepth = TileBlueDepth[idx];

			const float3 neighbor = float3(f16tof32(redGreen), f16tof32(redGreen >> 16), f16tof32(blueDepth));
			neighborhoodMin = min(neighborhoodMin, neighbor);
			neighborhoodMax = max(neighborhoodMax, neighbor);
			if (x == 0 && y == 0)
			{
				current = neighbor;
			}

			const float depth = f16tof32(blueDepth >> 16);
			if (depth < bestDepth)
			{
				bestDepth = depth;
				bestOffset = offset;
			}
		}
	}

	const float2 m = Motion[cin.DispatchThreadId.xy + bestOffset].xy;
	const float2 prevUV = uv + m;

	float4 prev = PrevFrame.SampleLevel(LinearSampler, prevUV, 0);
	prev.rgb = clamp(prev.rgb, neighborhoodMin, neighborhoodMax);

	float subpixelCorrection = frac(
		max(
			abs(m.x) * Taa.Dimensions.x,
			abs(m.y) * Taa.Dimensions.y
		)
	) * 0.5f;

	float blendfactor = saturate(lerp(0.05f, 0.8f, subpixelCorrection));
	blendfactor = IsSaturated(prevUV) ? blendfactor : 1.f;

#ifdef HDR_CORRECTION
	prev.rgb = Tonemap(prev.rgb);
	current.rgb = Tonemap(current.rgb);
#endif

	float3 resolved = lerp(prev.rgb, current.rgb, blendfactor);

#ifdef HDR_CORRECTION
	resolved.rgb = InverseTonemap(resolved.rgb);
#endif

	Output[texCoord] = float4(resolved, 1.f);
}
