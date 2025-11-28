#ifndef MATERIAL_H
#define MATERIAL_H

#include "common.hlsli"

static uint32 PackColor(uint32 r, uint32 g, uint32 b, uint32 a)
{
    return ((a & 0xFF) << 24) | ((b & 0xFF) << 16) | ((g & 0xFF) << 8) | (r & 0xFF);
}

static uint32 PackColor(float r, float g, float b, float a)
{
    return PackColor(
        (uint32)clamp(r * 255.f, 0.f, 255.f),
        (uint32)clamp(g * 255.f, 0.f, 255.f),
        (uint32)clamp(b * 255.f, 0.f, 255.f),
        (uint32)clamp(a * 255.f, 0.f, 255.f));
}

static uint32 PackColor(vec4 rgba)
{
    return PackColor(rgba.r, rgba.g, rgba.b, rgba.a);
}

static vec4 UnpackColor(uint32 c)
{
    const float mul = 1.f / 255.f;
    float r = (c & 0xFF) * mul;
    float g = ((c >> 8) & 0xFF) * mul;
    float b = ((c >> 16) & 0xFF) * mul;
    float a = ((c >> 24) & 0xFF) * mul;
    return vec4(r, g, b, a);
}

#define USE_ALBEDO_TEXTURE        (1 << 0)
#define USE_NORMAL_TEXTURE        (1 << 1)
#define USE_ROUGHNESS_TEXTURE     (1 << 2)
#define USE_METALLIC_TEXTURE      (1 << 3)
#define USE_AO_TEXTURE            (1 << 4)

struct PbrMaterialCb // 24 bytes.
{
    vec3 Emission;                              // Since emission can be HDR, we use full precision floats here.
    uint32 AlbedoTint;                          // RGBA packed into one uint32. Can only be between 0 and 1, which is why we are only using 8 bit per channel.
    uint32 RoughnessMetallicFlagsRefraction;    // 8 bit each.
    uint32 NormalMapStrengh;                    // Packed as a 16 bit float. 16 bit are still free! TODO: Maybe values between 0 and 1 are enough for normal map strength? Then 8 bit would suffice.

    void Initialize(vec4 albedo, vec3 emission, float roughness, float metallic, uint32 flags, float normalMapStrengh = 1.f, float refractionStrength = 0.f)
    {
        Emission = emission;
        AlbedoTint = PackColor(albedo);

        roughness = clamp(roughness, 0.f, 1.f);
        metallic = clamp(metallic, 0.f, 1.f);
        refractionStrength = clamp(refractionStrength, 0.f, 1.f);

        RoughnessMetallicFlagsRefraction =
            ((uint32)(roughness * 0xFF) << 24) |
			((uint32)(metallic * 0xFF) << 16) |
			(flags << 8) |
			((uint32)(refractionStrength * 0xFF) << 0);
        
        uint32 normalStrengthHalf;
#ifndef HLSL
        normalStrengthHalf = half(normalMapStrengh).h;
#else
        normalStrengthHalf = f32tof16(normalMapStrengh);
#endif
        normalMapStrengh = (normalStrengthHalf << 16);
    }

#ifndef HLSL
    PbrMaterialCb() {}

    PbrMaterialCb(vec4 albedo, vec3 emission, float roughness, float metallic, uint32 flags, float normalMapStrengh = 1.f, float refractionStrength = 0.f) 
    {
        Initialize(albedo, emission, roughness, metallic, flags, normalMapStrengh, refractionStrength);
    }
#endif

    vec4 GetAlbedo()
    {
        return UnpackColor(AlbedoTint);
    }

    float GetRoughnessOverride()
    {
        return ((RoughnessMetallicFlagsRefraction >> 24) & 0xff) / (float)0xff;
    }

    float GetMetallicOverride()
    {
        return ((RoughnessMetallicFlagsRefraction >> 16) & 0xff) / (float)0xff;
    }

    float GetNormalMapStrength()
    {
        uint32 normalStrengthHalf = NormalMapStrengh >> 16;

#ifndef HLSL
        return half((uint16)normalStrengthHalf);
#else
        return f16tof32(normalStrengthHalf);
#endif
    }

    float GetRefractionStrength()
    {
        return ((RoughnessMetallicFlagsRefraction >> 0) & 0xff) / (float)0xff;
    }

    uint32 GetFlags()
    {
        return (RoughnessMetallicFlagsRefraction >> 8) & 0xff;
    }
};

struct PbrDecalCb
{
    vec3 Position;
    uint32 AlbedoTint;			// RGBA packed into one uint32.
	vec3 Right;					// Scaled by half dimension.
	uint32 RoughnessOverrideMetallicOverride;
	vec3 Up;					// Scaled by half dimension.
	uint32 ViewportXY;			// Top left corner packed into 16 bits each.
	vec3 Forward;				// Scaled by half dimension.
	uint32 ViewportScale;       // Width and height packed into 16 bits each. 

    void Initialize(vec3 position, vec3 right, vec3 up, vec3 forward, vec4 albedo, float roughness, float metallic, vec4 viewport)
    {
        Position = position;
        Right = right;
        Up = up;
        Forward = forward;

		AlbedoTint = PackColor(albedo);

		uint32 r = (uint32)(roughness * 0xffff);
		uint32 m = (uint32)(metallic * 0xffff);
		RoughnessOverrideMetallicOverride = (r << 16) | m;

		uint32 x = (uint32)(viewport.x * 0xffff);
		uint32 y = (uint32)(viewport.y * 0xffff);
		uint32 w = (uint32)(viewport.z * 0xffff);
		uint32 h = (uint32)(viewport.w * 0xffff);
		ViewportXY = (x << 16) | y;
		ViewportScale = (w << 16) | h;
    }

#ifndef HLSL
    PbrDecalCb() {}

    PbrDecalCb(vec3 position, vec3 right, vec3 up, vec3 forward, vec4 albedo, float roughness, float metallic, vec4 viewport)
    {
        Initialize(position, right, up, forward, albedo, roughness, metallic, viewport);
    }
#endif

    vec4 GetAlbedo()
    {
        return UnpackColor(AlbedoTint);
    }

    float GetRoughnessOverride()
    {
        return (RoughnessOverrideMetallicOverride >> 16) / (float)0xffff;
    }

    float GetMetallicOverride()
    {
        return (RoughnessOverrideMetallicOverride & 0xffff) / (float)0xffff;
    }

    vec4 GetViewport()
    {
        float x = (ViewportXY >> 16) / (float)0xffff;
		float y = (ViewportXY & 0xffff) / (float)0xffff;
		float w = (ViewportScale >> 16) / (float)0xffff;
		float h = (ViewportScale & 0xffff) / (float)0xffff;
		return vec4(x, y, w, h);
    }
};

#endif