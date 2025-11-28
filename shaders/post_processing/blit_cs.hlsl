#include "../rs/post_processing_rs.hlsli"
#include "../common/cs.hlsli"

ConstantBuffer<BlitCb> Blit				: register(b0);
RWTexture2D<float4> Output				: register(u0);
Texture2D<float4> Input					: register(t0);
SamplerState LinearClampSampler			: register(s0);

[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(BLIT_RS)]
void main(CsInput cin)
{
	const float2 uv = (cin.DispatchThreadId.xy + float2(0.5f, 0.5f)) * Blit.InvDimensions;
	Output[cin.DispatchThreadId.xy] = Input.SampleLevel(LinearClampSampler, uv, 0);
}
