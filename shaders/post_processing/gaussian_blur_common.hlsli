#include "../common/cs.hlsli"
#include "../rs/post_processing_rs.hlsli"


ConstantBuffer<GaussianBlurCb> GaussianBlur : register(b0);

Texture2D<float4> Input		                : register(t0);
RWTexture2D<float4> Output		            : register(u0);
SamplerState LinearClampSampler             : register(s0);


[RootSignature(GAUSSIAN_BLUR_RS)]
[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
void main(CsInput cin)
{
    float2 uv = (cin.DispatchThreadId.xy + float2(0.5f, 0.5f)) * GaussianBlur.InvDimensions;

    uint directionIndex = GaussianBlur.DirectionAndSourceMipLevel >> 16;
    float2 direction = (directionIndex == 0) ? float2(1.f, 0.f) : float2(0.f, 1.f);
    direction *= GaussianBlur.InvDimensions;
    direction *= GaussianBlur.StepScale;

    uint sourceMipLevel = GaussianBlur.DirectionAndSourceMipLevel & 0xFFFF;
    float4 color = Input.SampleLevel(LinearClampSampler, uv, sourceMipLevel) * BlurWeights[0];

    [unroll]
    for (int i = 1; i < NUM_WEIGHTS; ++i)
    {
        float2 normalizedOffset = KernelOffsets[i] * direction;
        color += Input.SampleLevel(LinearClampSampler, uv + normalizedOffset, sourceMipLevel) * BlurWeights[i];
        color += Input.SampleLevel(LinearClampSampler, uv - normalizedOffset, sourceMipLevel) * BlurWeights[i];
    }

    Output[cin.DispatchThreadId.xy] = color;
}