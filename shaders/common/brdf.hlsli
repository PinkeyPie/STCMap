#ifndef BRDF_HLSLI
#define BRDG_HLSLI

// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// We should really make an optimization pass over this code. Many terms are computed multiple times.

#ifndef PI
#define PI 3.14159265359f
#endif

#ifndef ONE_OVER_PI
#define ONE_OVER_PI (1.f / PI)
#endif

static float3 FresnelSchlick(float LdotH, float3 R0)
{
    return R0 + (float3(1.f, 1.f, 1.f) - R0) * pow(1.f - LdotH, 5.f);
}

static float3 FresnelSchlickRoughness(float LdotH, float3 R0, float roughness)
{
    float v = 1.f - roughness;
    return R0 + (max(float3(v, v, v), R0) - R0) * pow(1.f - LdotH, 5.f);
}

static float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.f);
    float NdotH2 = NdotH * NdotH;
    
    float nominator = a2;
    float denominator = (NdotH2 * (a2 - 1.f) + 1.f);
    denominator = PI * denominator * denominator;
    
    return nominator / max(denominator, 0.001f);
}

static float GeometrySchlickGGX(float NdotV, float roughness)
{
    // float r = (roughness + 1.f);
    // float k = (r * r) * 0.125;
    float a = roughness;
    float k = (a * a) / 2.f;
    
    float nominator = NdotV;
    float denominator = NdotV * (1.f - k) + k;
    
    return nominator / denominator;
}

static float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.f);
    float NdotL = max(dot(N, L), 0.f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

static float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xaaaaaaaau) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xccccccccu) >> 2u);
    bits = ((bits & 0x0f0f0f0fu) << 4u) | ((bits & 0xf0f0f0f0u) >> 4u);
    bits = ((bits & 0x00ff00ffu) << 8u) | ((bits & 0xff00ff00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

static float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

static float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.f * PI * Xi.x;
    float cosTheta = sqrt((1.f - Xi.y) / (1.f + (a * a - 1.f) * Xi.y));
    float sinTheta = sqrt(1.f - cosTheta * cosTheta);

    // from spherical coordinates to cartesian coordinates
    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // from tangent-space H vector to world-space sample vector
    float3 up = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

static float3 CalculateDirectLighting(float3 albedo, float3 radiance, float3 N, float3 L, float3 V, float3 R0, float roughness, float metallic) 
{
    float3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.f);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    float3 F  = FresnelSchlick(max(dot(H, V), 0.f), R0);

    float kS = F;
    float3 kD = float3(1.f, 1.f, 1.f) - kS;
    kD *= 1.f - metallic;

    float NdotL = max(dot(N, L), 0.f);
    float3 numerator = NDF * G * F;
    float denominator = 4.f * NdotV * NdotL + 0.001f;
    float3 specular = numerator / max(denominator, 0.001f);

    return (kD * albedo * ONE_OVER_PI + specular) * radiance * NdotL;
}

#endif