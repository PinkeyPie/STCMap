#include "rs/present_rs.hlsli"

struct PsInput
{
    float2 uv : TEXCOORDS;
};

ConstantBuffer<TonemapCb> Tonemap : register(b0);
ConstantBuffer<PresentCb> Present : register(b1);
SamplerState TexSampler           : register(s0);
Texture2D<float4> Tex             : register(t0);

#define SDR 0
#define HDR 1

static float3 LinearToSRGB(float3 color)
{
    // Approximately pow(color, 1.0 / 2.2).
	return color < 0.0031308f ? 12.92f * color : 1.055f * pow(abs(color), 1.f / 2.4f) - 0.055f;
}

static float3 SRGBToLinear(float3 color)
{
    // Approximately pow(color, 2.2).
	return color < 0.04045f ? color / 12.92f : pow(abs(color + 0.055f) / 1.055f, 2.4f);
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

// https://www.slideshare.net/ozlael/hable-john-uncharted2-hdr-lighting/142
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

[RootSignature(PRESENT_RS)]
float4 main(PsInput pin) : SV_Target
{
    float4 scene = Tex.Sample(TexSampler, pin.uv);

    scene.rgb = FilmicTonemapping(scene.rgb);

    if(Present.DisplayMode == SDR)
    {
        scene.rgb = LinearToSRGB(scene.rgb);
    }
    else if(Present.DisplayMode == HDR)
    {
        const float st2084max = 10000.f;
        const float hdrScalar = Present.StandardNits / st2084max;

        // The HDR scene is in Rec.709, but the display is Rec.2020
        scene.rgb = Rec709ToRec2020(scene.rgb);

        // Apply the ST.2084 curve to the scene
        scene.rgb = LinearToST2084(scene.rgb * hdrScalar);
    }

    scene.a = 1.f;
    return scene;
}