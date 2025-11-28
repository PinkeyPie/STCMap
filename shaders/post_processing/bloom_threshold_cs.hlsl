#include "../rs/post_processing_rs.hlsli"
#include "../common/cs.hlsli"

ConstantBuffer<BloomThresholdCb> BloomCb : register(b0);

RWTexture2D<float4> Output				 : register(u0);
Texture2D<float4> Input					 : register(t0);

SamplerState LinearClampSampler			 : register(s0);


[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(BLOOM_THRESHOLD_RS)]
void main(CsInput cin)
{
	const float2 uv = cin.DispatchThreadId.xy + float2(0.5f, 0.5f);

	float3 color = (float3)0.f;

	const float2 invDimensions = BloomCb.InvDimensions;
	const float offset = 0.25f;
	color += Input.SampleLevel(LinearClampSampler, (uv + float2(-offset, -offset)) * invDimensions, 0).rgb;
	color += Input.SampleLevel(LinearClampSampler, (uv + float2(offset, -offset)) * invDimensions, 0).rgb;
	color += Input.SampleLevel(LinearClampSampler, (uv + float2(-offset, offset)) * invDimensions, 0).rgb;
	color += Input.SampleLevel(LinearClampSampler, (uv + float2(offset, offset)) * invDimensions, 0).rgb;

	color *= 0.25f;
	color = max(0, color - BloomCb.Threshold.xxx);

	Output[cin.DispatchThreadId.xy] = float4(color, 1.f);
}
