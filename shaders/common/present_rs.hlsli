#ifndef PRESENT_RS_HLSLI
#define PRESENT_RS_HLSLI

#include "common.hlsli"

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

struct ObjectCb {
    mat4 World;
    mat4 View;
    mat4 Proj;
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

#define PRESENT_RS \
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | " \
"ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | " \
"DENY_VERTEX_SHADER_ROOT_ACCESS | " \
"DENY_HULL_SHADER_ROOT_ACCESS | " \
"DENY_DOMAIN_SHADER_ROOT_ACCESS | "\
"DENY_GEOMETRY_SHADER_ROOT_ACCESS)," \
"RootConstants(num32BitConstants=8, b0, visibility=SHADER_VISIBILITY_PIXEL)"\

#define PRESENT_RS_MVP_CBV_PARAM 0

#endif