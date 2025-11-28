#include "../rs/outline_rs.hlsli"

struct PsInput
{
	float2 UV				: TEXCOORDS;
	float4 ScreenPosition	: SV_POSITION;
};

ConstantBuffer<OutlineDrawerCb> Outline : register(b0);

Texture2D<uint2> Stencil	: register(t0);


static const int selected = (1 << 0); // TODO: This must be the same as in renderer.

static uint SampleAt(int2 texCoords)
{
	uint result = 1;
	if (texCoords.x >= 0 && texCoords.y >= 0 && texCoords.x < Outline.Width && texCoords.y < Outline.Height)
	{
		result = (Stencil[texCoords].y & selected) != 0;
	}
	return result;
}

[RootSignature(OUTLINE_DRAWER_RS)]
float4 main(PsInput pin) : SV_TARGET
{
	int2 texCoords = int2(pin.ScreenPosition.xy);

	uint s = 0;

	[unroll]
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			s += SampleAt(texCoords + int2(x, y));
		}
	}

	if (s == 25)
	{
		discard;
	}

	return float4(1, 0, 0.f, 1.f);
}