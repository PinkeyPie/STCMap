#include "../rs/post_processing_rs.hlsli"
#include "../common/cs.hlsli"

ConstantBuffer<BloomCombineCb> BloomCb	: register(b0);

RWTexture2D<float4> Output				: register(u0);
Texture2D<float4> Scene					: register(t0);
Texture2D<float4> Bloom					: register(t1);

SamplerState LinearClampSampler			: register(s0);


[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(BLOOM_COMBINE_RS)]
void main(CsInput cin)
{
	const float2 uv = (cin.DispatchThreadId.xy + float2(0.5f, 0.5f)) * BloomCb.InvDimensions;

	float3 color = (float3)0.f;

	color += Bloom.SampleLevel(LinearClampSampler, uv, 1.5f).rgb;
	color += Bloom.SampleLevel(LinearClampSampler, uv, 3.5f).rgb;
	color += Bloom.SampleLevel(LinearClampSampler, uv, 4.5f).rgb;

	color /= 3.f;

	color *= BloomCb.Strength;

	Output[cin.DispatchThreadId.xy] = float4(Scene[cin.DispatchThreadId.xy].rgb + color, 1.f);
}
