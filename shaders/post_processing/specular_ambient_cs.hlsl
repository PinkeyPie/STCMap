#include "../common/cs.hlsli"
#include "../rs/post_processing_rs.hlsli"
#include "../common/brdf.hlsli"
#include "../common/normal.hlsli"
#include "../common/camera.hlsli"

ConstantBuffer<SpecularAmbientCb> AmbientCb : register(b0);
ConstantBuffer<CameraCb> Camera			    : register(b1);

RWTexture2D<float4> Output					: register(u0);

Texture2D<float4> Scene						: register(t0);
Texture2D<float2> WorldNormals				: register(t1);
Texture2D<float4> Reflectance				: register(t2);

Texture2D<float4> Reflection				: register(t3);


TextureCube<float4> EnvironmentTexture		: register(t4);
Texture2D<float2> Brdf						: register(t5);

SamplerState ClampSampler					: register(s0);


[numthreads(POST_PROCESSING_BLOCK_SIZE, POST_PROCESSING_BLOCK_SIZE, 1)]
[RootSignature(SPECULAR_AMBIENT_RS)]
void main(CsInput cin)
{
	float2 uv = (cin.DispatchThreadId.xy + 0.5f) * AmbientCb.InvDimensions;

	float4 color = Scene[cin.DispatchThreadId.xy];
	if (color.a > 0.f) // Alpha of 0 indicates sky.
	{
		float4 refl = Reflectance[cin.DispatchThreadId.xy];

		SurfaceInfo surface;
		surface.Albedo = 0.f.xxxx;
		surface.Metallic = 0.f;
		surface.N = normalize(UnpackNormal(WorldNormals[cin.DispatchThreadId.xy]));
		surface.V = -normalize(RestoreWorldDirection(uv, Camera.InvViewProj, Camera.Position.xyz));
		surface.Roughness = clamp(refl.a, 0.01f, 0.99f);

		surface.InferRemainingProperties();

		float4 ssr = Reflection.SampleLevel(ClampSampler, uv, 0);

		float3 specular = SpecularIBL(refl.rgb, surface, EnvironmentTexture, Brdf, ClampSampler);
		specular = lerp(specular, ssr.rgb, ssr.a);

		color.rgb += specular;
	}
	Output[cin.DispatchThreadId.xy] = color;
}