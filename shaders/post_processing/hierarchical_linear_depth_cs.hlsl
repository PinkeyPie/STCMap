#include "../common/cs.hlsli"
#include "../rs/post_processing_rs.hlsli"
#include "../common/camera.hlsli"

// This shader outputs the depth in world units!

ConstantBuffer<HierarchicalLinearDepthCb> DepthCb	: register(b0);
ConstantBuffer<CameraCb> Camera					    : register(b1);

RWTexture2D<float> OutputMip0						: register(u0);
RWTexture2D<float> OutputMip1						: register(u1);
RWTexture2D<float> OutputMip2						: register(u2);
RWTexture2D<float> OutputMip3						: register(u3);
RWTexture2D<float> OutputMip4						: register(u4);
RWTexture2D<float> OutputMip5						: register(u5);

Texture2D<float> Input								: register(t0);

SamplerState LinearClampSampler						: register(s0);

groupshared float tile[POST_PROCESSING_BLOCK_SIZE][POST_PROCESSING_BLOCK_SIZE];

[RootSignature(HIERARCHICAL_LINEAR_DEPTH_RS)]
[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
void main(CsInput cin)
{
    const uint2 groupThreadID = cin.GroupThreadId.xy;
	const uint2 dispatchThreadID = cin.DispatchThreadId.xy;

	float2 uv = (dispatchThreadID + float2(0.5f, 0.5f)) * DepthCb.InvDimensions;

	float4 depths = Input.Gather(LinearClampSampler, uv);
	//float4 depths = float4(
	//	input[dispatchThreadID * 2 + uint2(0, 0)],
	//	input[dispatchThreadID * 2 + uint2(1, 0)],
	//	input[dispatchThreadID * 2 + uint2(0, 1)],
	//	input[dispatchThreadID * 2 + uint2(1, 1)]
	//);
	float maxdepth = max(depths.x, max(depths.y, max(depths.z, depths.w)));
	tile[groupThreadID.x][groupThreadID.y] = maxdepth;

	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x % 2 == 0 && groupThreadID.y % 2 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], 
			max(tile[groupThreadID.x + 1][groupThreadID.y], 
			max(tile[groupThreadID.x][groupThreadID.y + 1], tile[groupThreadID.x + 1][groupThreadID.y + 1])));
		tile[groupThreadID.x][groupThreadID.y] = maxdepth;
	}
	GroupMemoryBarrierWithGroupSync();

	const float4 lineardepths = float4
	(
		DepthBufferDepthToEyeDepth(depths.x, Camera.ProjectionParams),
		DepthBufferDepthToEyeDepth(depths.y, Camera.ProjectionParams),
		DepthBufferDepthToEyeDepth(depths.z, Camera.ProjectionParams),
		DepthBufferDepthToEyeDepth(depths.w, Camera.ProjectionParams)
	);

    OutputMip0[dispatchThreadID.xy * 2 + uint2(0, 0)] = lineardepths.x;
	OutputMip0[dispatchThreadID.xy * 2 + uint2(1, 0)] = lineardepths.y;
	OutputMip0[dispatchThreadID.xy * 2 + uint2(0, 1)] = lineardepths.z;
	OutputMip0[dispatchThreadID.xy * 2 + uint2(1, 1)] = lineardepths.w;

	maxdepth = max(lineardepths.x, max(lineardepths.y, max(lineardepths.z, lineardepths.w)));
	tile[groupThreadID.x][groupThreadID.y] = maxdepth;
	OutputMip1[dispatchThreadID.xy] = maxdepth;
	GroupMemoryBarrierWithGroupSync();

    if (groupThreadID.x % 2 == 0 && groupThreadID.y % 2 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], max(tile[groupThreadID.x + 1][groupThreadID.y], max(tile[groupThreadID.x][groupThreadID.y + 1], tile[groupThreadID.x + 1][groupThreadID.y + 1])));
		tile[groupThreadID.x][groupThreadID.y] = maxdepth;
		OutputMip2[dispatchThreadID.xy / 2] = maxdepth;
	}
	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x % 4 == 0 && groupThreadID.y % 4 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], max(tile[groupThreadID.x + 2][groupThreadID.y], max(tile[groupThreadID.x][groupThreadID.y + 2], tile[groupThreadID.x + 2][groupThreadID.y + 2])));
		tile[groupThreadID.x][groupThreadID.y] = maxdepth;
		OutputMip3[dispatchThreadID.xy / 4] = maxdepth;
	}
	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x % 8 == 0 && groupThreadID.y % 8 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], max(tile[groupThreadID.x + 4][groupThreadID.y], max(tile[groupThreadID.x][groupThreadID.y + 4], tile[groupThreadID.x + 4][groupThreadID.y + 4])));
		tile[groupThreadID.x][groupThreadID.y] = maxdepth;
		OutputMip4[dispatchThreadID.xy / 8] = maxdepth;
	}
	GroupMemoryBarrierWithGroupSync();

	if (groupThreadID.x % 16 == 0 && groupThreadID.y % 16 == 0)
	{
		maxdepth = max(tile[groupThreadID.x][groupThreadID.y], max(tile[groupThreadID.x + 8][groupThreadID.y], max(tile[groupThreadID.x][groupThreadID.y + 8], tile[groupThreadID.x + 8][groupThreadID.y + 8])));
		OutputMip5[dispatchThreadID.xy / 16] = maxdepth;
	}
}