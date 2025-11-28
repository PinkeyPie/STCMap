#include "../rs/flat_simple_rs.hlsli"
#include "../common/camera.hlsli"

ConstantBuffer<FlatSimpleColorCb> ColorCb : register(b1);
ConstantBuffer<CameraCb> Camera		: register(b2);

struct PsInput
{
	float3 WorldPosition	: WORLD_POSITION;
	float3 WorldNormal		: WORLD_NORMAL;
};

[RootSignature(FLAT_SIMPLE_RS)]
float4 main(PsInput pin) : SV_TARGET
{
	float ndotv = saturate(dot(normalize(Camera.Position.xyz - pin.WorldPosition), normalize(pin.WorldNormal))) * 0.8 + 0.2;
	return ndotv * ColorCb.Color;
}