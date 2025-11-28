#include "../common/camera.hlsli"

struct PsInput
{
	float3 WorldPos : POSITION;
	float3 Normal	: NORMAL;
}; 

ConstantBuffer<CameraCb> Camera	: register(b1);

SamplerState TexSampler				: register(s0, space1);
TextureCube<float4> Tex				: register(t0, space1);

float4 main(PsInput pin) : SV_TARGET
{
	float3 V = normalize(pin.WorldPos - Camera.Position.xyz);
	float3 N = normalize(pin.Normal);

	// Compute reflection and refraction vectors.
	const float ETA = 1.12f;
	float c = dot(V, N);
	float d = ETA * c;
	float k = saturate(d * d + (1.f - ETA * ETA));
	float3 reflVec = V - 2.f * c * N;
	float3 refrVec = ETA * V - (d + sqrt(k)) * N;

	// Sample and blend.
	float3 refl = Tex.Sample(TexSampler, reflVec).rgb;
	float3 refr = Tex.Sample(TexSampler, refrVec).rgb;
	float3 sky = lerp(refl, refr, k);

	// Add a cheap and fake bubble color effect.
	float3 bubbleTint = 0.25f * pow(1.f - k, 5.f) * abs(N);

	return float4(sky + bubbleTint, 1.f);
}