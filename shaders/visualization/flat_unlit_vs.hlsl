#define RS\
"RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | DENY_HULL_SHADER_ROOT_ACCESS | "\
    "DENY_DOMAIN_SHADER_ROOT_ACCESS | DENY_GEOMETRY_SHADER_ROOT_ACCESS | "\
    "DENY_PIXEL_SHADER_ROOT_ACCESS),"\
"RootConstants(num32BitConstants=16, b0, visibility=SHADER_VISIBILITY_VERTEX)"

struct TransformCb
{
    float4x4 ModelViewProj;
};

ConstantBuffer<TransformCb> Transform : register(b0);

struct VsInput
{
    float3 Position : POSITION;
};

struct VsOutput
{
    float4 Position : SV_Position;
};

[RootSignature(RS)]
VsOutput main(VsInput vin) 
{
    VsOutput vout;
    vout.Position = mul(Transform.ModelViewProj, float4(vin.Position, 1.f));
    return vout;
}