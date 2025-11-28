#include "../common/cs.hlsli"
#include "../rs/post_processing_rs.hlsli"


ConstantBuffer<TonemapCb> Tonemap	: register(b0);
RWTexture2D<float4> Output	: register(u0);
Texture2D<float4>	Input	: register(t0);

static float3 AcesFilmic(float3 x, float A, float B, float C, float D, float E, float F)
{
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
}

static float3 FilmicTonemapping(float3 color)
{
	color *= exp2(Tonemap.Exposure);

	return AcesFilmic(color, Tonemap.A, Tonemap.B, Tonemap.C, Tonemap.D, Tonemap.E, Tonemap.F) /
		AcesFilmic(Tonemap.LinearWhite, Tonemap.A, Tonemap.B, Tonemap.C, Tonemap.D, Tonemap.E, Tonemap.F);
}

[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(TONEMAP_RS)]
void main(CsInput cin)
{
	Output[cin.DispatchThreadId.xy] = float4(
		FilmicTonemapping(
			Input[cin.DispatchThreadId.xy].rgb
		)
		, 1.f);
}
