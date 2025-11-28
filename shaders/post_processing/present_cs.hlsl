#include "../common/cs.hlsli"
#include "../rs/post_processing_rs.hlsli"

ConstantBuffer<PresentCb> Present	: register(b0);
RWTexture2D<float4> Output	: register(u0);
Texture2D<float4>	Input	: register(t0);


static float3 LinearToSRGB(float3 color)
{
	// Approximately pow(color, 1.0 / 2.2).
	return color.r < 0.0031308f && color.g < 0.0031308f && color.b < 0.0031308f ? 12.92f * color : 1.055f * pow(abs(color), 1.f / 2.4f) - 0.055f;
}

static float3 SRGBToLinear(float3 color)
{
	// Approximately pow(color, 2.2).
	return color.r < 0.04045f && color.g < 0.04045f && color.b < 0.04045f ? color / 12.92f : pow(abs(color + 0.055f) / 1.055f, 2.4f);
}

static float3 Rec709ToRec2020(float3 color)
{
	static const float3x3 conversion =
	{
		0.627402f, 0.329292f, 0.043306f,
		0.069095f, 0.919544f, 0.011360f,
		0.016394f, 0.088028f, 0.895578f
	};
	return mul(conversion, color);
}

static float3 Rec2020ToRec709(float3 color)
{
	static const float3x3 conversion =
	{
		1.660496f, -0.587656f, -0.072840f,
		-0.124547f, 1.132895f, -0.008348f,
		-0.018154f, -0.100597f, 1.118751f
	};
	return mul(conversion, color);
}

static float3 LinearToST2084(float3 color)
{
	float m1 = 2610.f / 4096.f / 4.f;
	float m2 = 2523.f / 4096.f * 128.f;
	float c1 = 3424.f / 4096.f;
	float c2 = 2413.f / 4096.f * 32.f;
	float c3 = 2392.f / 4096.f * 32.f;
	float3 cp = pow(abs(color), m1);
	return pow((c1 + c2 * cp) / (1.f + c3 * cp), m2);
}

[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(PRESENT_RS)]
void main(CsInput cin)
{
	float3 scene = Input[cin.DispatchThreadId.xy + int2(0, 0)].rgb;

	if (Present.SharpenStrength > 0.f)
	{
		float3 top = Input[cin.DispatchThreadId.xy + int2(0, -1)].rgb;
		float3 left = Input[cin.DispatchThreadId.xy + int2(-1, 0)].rgb;
		float3 right = Input[cin.DispatchThreadId.xy + int2(1, 0)].rgb;
		float3 bottom = Input[cin.DispatchThreadId.xy + int2(0, 1)].rgb;

		scene = max(scene + (4.f * scene - top - bottom - left - right) * Present.SharpenStrength, 0.f);
	}

	if (Present.DisplayMode == PRESENT_SDR)
	{
		scene = LinearToSRGB(scene);
	}
	else if (Present.DisplayMode == PRESENT_HDR)
	{
		const float st2084max = 10000.f;
		const float hdrScalar = Present.StandardNits / st2084max;

		// The HDR scene is in Rec.709, but the display is Rec.2020.
		scene = Rec709ToRec2020(scene);

		// Apply the ST.2084 curve to the scene.
		scene = LinearToST2084(scene * hdrScalar);
	}

	Output[cin.DispatchThreadId.xy + int2(Present.Offset >> 16, Present.Offset & 0xFFFF)] = float4(scene, 1.f);
}
