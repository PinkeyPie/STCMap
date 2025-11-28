#ifndef BRDF_HLSLI
#define BRDF_HLSLI

#include "math.hlsli"
#include "random.hlsli"
#include "light_source.hlsli"

struct SurfaceInfo
{
    // Set from outside
    float3 P;
    float3 N;
    float3 V;

    float4 Albedo;
    float Roughness;
    float Metallic;
    float3 Emission;

    // Inferred from properties above.
    float AlphaRoughness;
    float AlphaRoughnessSquared;
    float NdotV;
    float3 R0;
    float3 R;

    inline void InferRemainingProperties()
    {
        AlphaRoughness = Roughness * Roughness;
        AlphaRoughnessSquared = AlphaRoughness * AlphaRoughness;
        NdotV = saturate(dot(N, V));
        R0 = lerp(float3(0.04f, 0.04f, 0.04f), Albedo.xyz, Metallic);
        R = reflect(-V, N);
    }
};

struct LightInfo
{
    float3 L;
    float3 H;
    float NdotL;
    float NdotH;
    float LdotH;
    float VdotH;

    float3 Radiance;
    float DistanceToLight;

    inline void Initialize(SurfaceInfo surface, float3 l, float3 rad)
    {
        L = l;
        H = normalize(L + surface.V);

        NdotL = saturate(dot(surface.N, L));
        NdotH = saturate(dot(surface.N, H));
        LdotH = saturate(dot(L, H));
        VdotH = saturate(dot(surface.V, H));

        Radiance = rad;
    }

    inline void InitializeFromPointLight(SurfaceInfo surface, PointLightCb pl)
    {
        float3 L = pl.Position - surface.P;
        DistanceToLight = length(L);
        L /= DistanceToLight;

        Initialize(surface, L, pl.Radiance * GetAttenuation(DistanceToLight, pl.Radius) * LIGHT_RADIANCE_SCALE);
    }

    inline void InitializeFromPointOnSphereLight(SurfaceInfo surface, PointLightCb pl, float radius, inout uint randSeed)
    {
        float3 randomPointOnLight = pl.Position + GetRandomPointOnSphere(randSeed, radius);

        float3 L = randomPointOnLight - surface.P;
        DistanceToLight = length(L);
        L /= DistanceToLight;

        Initialize(surface, L, pl.Radiance * GetAttenuation(DistanceToLight, pl.Radius) * LIGHT_RADIANCE_SCALE);
    }

    inline void InitializeFromSpotLight(SurfaceInfo surface, SpotLightCb sl)
    {
        float3 L = (sl.Position - surface.P);
        DistanceToLight = length(L);
        L /= DistanceToLight;

        float innerCutoff = sl.GetInnerCutoff();
        float outerCutoff = sl.GetOuterCutoff();
        float epsilon = innerCutoff - outerCutoff;

        float theta = dot(-L, sl.Direction);
        float attenuation = GetAttenuation(DistanceToLight, sl.MaxDistance);
        float intensity = saturate((theta - outerCutoff) / epsilon) * attenuation;

        Initialize(surface, L, sl.Radiance * intensity * LIGHT_RADIANCE_SCALE);
    }
};

// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html

// ----------------------------------------
// FRESNEL (Surface becomes more reflective when seen from a grazing angle).
// ----------------------------------------

static float3 FresnelSchlick(float LdotH, float3 R0)
{
	return R0 + (float3(1.f, 1.f, 1.f) - R0) * pow(1.f - LdotH, 5.f);
}

static float3 FresnelSchlickRoughness(float LdotH, float3 R0, float roughness)
{
	float v = 1.f - roughness;
	return R0 + (max(float3(v, v, v), R0) - R0) * pow(1.f - LdotH, 5.f);
}

// ----------------------------------------
// DISTRIBUTION (Microfacets' orientation based on roughness).
// ----------------------------------------
static float DistributionGGX(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;

    float d = (NdotH2 * (a2 - 1.f) + 1.f);
    return a2 / max(d * d * PI, 0.001f);
}

static float DistributionGGX(SurfaceInfo surface, LightInfo light)
{
    float NdotH = light.NdotH;
    float NdotH2 = NdotH * NdotH;
    float a2 = surface.AlphaRoughnessSquared;
    float d = (NdotH2 * (a2 - 1.f) + 1.f);
    return a2 / max(d * d * PI, 0.001f);
}

// ----------------------------------------
// GEOMETRIC MASKING (Microfacets may shadow each-other).
// ----------------------------------------

static float GeometrySmith(float NdotL, float NdotV, float roughness)
{
    float k = (roughness * roughness) * 0.5f;

    float ggx2 = NdotV / (NdotV * (1.f - k) + k);
    float ggx1 = NdotL / (NdotL * (1.f - k) + k);

    return ggx1 * ggx2;
}

static float GeometrySmith(SurfaceInfo surface, LightInfo light)
{
    float k = surface.AlphaRoughness * 0.5f;

    float ggx2 = surface.NdotV / (surface.NdotV * (1.f - k) + k);
    float ggx1 = light.NdotL / (light.NdotL * (1.f - k) + k);

    return ggx1 * ggx2;
}

// ----------------------------------------
// IMPORTANCE SAMPLING
// ----------------------------------------

// When using this function to sample, the probability density is:
//      pdf = D * NdotH / (4 * HdotV)
static float3 ImportanceSampleGGX(inout uint randSeed, float3 N, float roughness)
{
    // Get our uniform random numbers.
    float2 randVal = float2(NextRand(randSeed), NextRand(randSeed));

    // Get an orthonormal basis from the normal.
    float3 B = GetPerpendicularVector(N);
    float3 T = cross(B, N);

    // GGX NDF sampling.
    float a2 = roughness * roughness;
    float cosThetaH = sqrt(max(0.f, (1.f - randVal.x) / ((a2 - 1.f) * randVal.x + 1.f)));
    float sinThetaH = sqrt(max(0.f, 1.f - cosThetaH * cosThetaH));
    float phiH = randVal.y * PI * 2.f;

    // Get our GGX NDF sample (i.e., the half vector).
    return T * (sinThetaH * cos(phiH)) + B * (sinThetaH * sin(phiH)) + N * cosThetaH;
}

// Call this with a hammersley distribution as Xi.
static float4 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;

	float phi = 2.f * PI * Xi.x;
	float cosTheta = sqrt((1.f - Xi.y) / (1.f + (a2 - 1.f) * Xi.y));
	float sinTheta = sqrt(1.f - cosTheta * cosTheta);

	// From spherical coordinates to cartesian coordinates.
	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	// From tangent-space vector to world-space sample vector.
	float3 up = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	H = tangent * H.x + bitangent * H.y + N * H.z;

	float d = (cosTheta * a2 - cosTheta) * cosTheta + 1.f;
	float D = a2 / (PI * d * d);
	float pdf = D * cosTheta;
	return float4(normalize(H), pdf);
}

// ----------------------------------------
// LIGHTING COMPUTATION.
// ----------------------------------------
static float3 DiffuseIBL(float3 kd, SurfaceInfo surface, TextureCube<float4> irradianceTexture, SamplerState clampSampler)
{
	float3 irradiance = irradianceTexture.SampleLevel(clampSampler, surface.N, 0).rgb;
	return kd * irradiance;
}

static float3 SpecularIBL(float3 F, SurfaceInfo surface, TextureCube<float4> environmentTexture, Texture2D<float2> brdf, SamplerState clampSampler)
{
    uint width, height, numMipLevels;
    environmentTexture.GetDimensions(0, width, height, numMipLevels);
    float lod = surface.Roughness * float(numMipLevels - 1);

    float3 prefilterredColor = environmentTexture.SampleLevel(clampSampler, surface.R, lod).rgb;
    float2 envBRDF = brdf.SampleLevel(clampSampler, float2(surface.Roughness, surface.NdotV), 0);
    float3 specular = prefilterredColor * (F * envBRDF.x + envBRDF.y);

    return specular;
}

struct AmbientFactors
{
    float3 Kd;
    float3 Ks;
};

static AmbientFactors GetAmbientFactors(SurfaceInfo surface)
{
    float3 F = FresnelSchlickRoughness(surface.NdotV, surface.R0, surface.Roughness);
    float3 kd = float3(1.f, 1.f, 1.f) - F;
    kd *= 1.f - surface.Metallic;

    AmbientFactors result = { kd, F };
    return result;
}

struct LightContribution
{
    float3 Diffuse;
    float3 Specular;

    void Add(LightContribution other, float visibility)
    {
        Diffuse += other.Diffuse * visibility;
        Specular += other.Specular * visibility;
    }

    float4 Evaluate(float4 albedo)
    {
        float3 c = albedo.rgb * Diffuse + Specular;
        return float4(c, albedo.a);
    }
};

static LightContribution CalculateAmbientLight(SurfaceInfo surface, TextureCube<float4> irradianceTexture, TextureCube<float4> environmentTexture, Texture2D<float2> brdf, SamplerState clampSampler)
{
    AmbientFactors factors = GetAmbientFactors(surface);

    float3 diffuse = DiffuseIBL(factors.Kd, surface, irradianceTexture, clampSampler);
    float3 specular = SpecularIBL(factors.Ks, surface, environmentTexture, brdf, clampSampler);

    LightContribution result = { diffuse, specular };
    return result;
}

static LightContribution CalculateDirectLighting(SurfaceInfo surface, LightInfo light)
{
    float D = DistributionGGX(surface, light);
    float G = GeometrySmith(surface, light);
    float3 F = FresnelSchlick(light.VdotH, surface.R0);

    float3 kD = float3(1.f, 1.f, 1.f) - F;
    kD *= 1.f - surface.Metallic;
    float3 diffuse = kD * INV_PI * light.Radiance * light.NdotL;

    float3 numerator = D * G * F;
    float denominator = 4.f * surface.NdotV * light.NdotL;
    float3 specular = numerator / max(denominator, 0.001f) * light.Radiance * light.NdotL;

    LightContribution result = { diffuse, specular };
    return result;
}

#endif