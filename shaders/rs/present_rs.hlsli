#ifndef PRESENT_RS_HLSLI
#define PRESENT_RS_HLSLI

#include "../common/common.hlsli"

struct TonemapCb
{
    float A; // Shoulder strength
    float B; // Linear strength
    float C; // Linear angle
    float D; // Toe stength
    float E; // Toe Numerator
    float F; // Toe Denominator
    // Note E/F = Toe angle
    float LinearWhite;
    
    float Exposure;
};

struct PresentCb 
{
    uint32 DisplayMode;
    float StandardNits;
};

static TonemapCb DefaultTonemapParameters()
{
    TonemapCb result;
    result.Exposure = 0.5f;
    result.A = 0.22f;
    result.B = 0.3f;
    result.C = 0.1f;
    result.D = 0.2f;
    result.E = 0.01f;
    result.F = 0.3f;
    result.LinearWhite = 11.2f;
    return result;
}

static float AcesFilmic(float x, float A, float B, float C, float D, float E, float F)
{
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - (E / F);
}

static float FilmicTonemapping(float color, TonemapCb tonemap)
{
    float expExposure = exp2(tonemap.Exposure);
    color *= expExposure;

    float r = AcesFilmic(color, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F) /
		AcesFilmic(tonemap.LinearWhite, tonemap.A, tonemap.B, tonemap.C, tonemap.D, tonemap.E, tonemap.F);

    return r;
}

#define PRESENT_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | " \
"ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | " \
"DENY_VERTEX_SHADER_ROOT_ACCESS | " \
"DENY_HULL_SHADER_ROOT_ACCESS | " \
"DENY_DOMAIN_SHADER_ROOT_ACCESS | "\
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=8, b0, visibility=SHADER_VISIBILITY_PIXEL),"\
"RootConstants(num32BitConstants=2, b1, visibility=SHADER_VISIBILITY_PIXEL),"\
"StaticSampler(s0,"\
    "addressU=TEXTURE_ADDRESS_CLAMP," \
    "addressV=TEXTURE_ADDRESS_CLAMP," \
    "addressW=TEXTURE_ADDRESS_CLAMP," \
    "filter=FILTER_MIN_MAG_MIP_POINT," \
    "visibility=SHADER_VISIBILITY_PIXEL)," \
"DescriptorTable(SRV(t0, numDescriptors=1), visibility=SHADER_VISIBILITY_PIXEL)"

#define PresentRsTonemap 0
#define PresentRsPresent 1
#define PresentRsTex     2

#endif