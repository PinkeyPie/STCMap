#include "../rs/sky_rs.hlsli"

ConstantBuffer<SkyCb> Sky : register(b0);

struct VsInput
{
    uint VertexId : SV_VertexID;
};

struct VsOutput
{
    float3 UV       : TEXCOORDS;
    float4 Position : SV_Position;
};

VsOutput main(VsInput vin)
{
    VsOutput vout;

    uint b = 1 << vin.VertexId;
    float3 pos = float3(
		(0x287a & b) != 0, 
		(0x02af & b) != 0, 
		(0x31e3 & b) != 0
	) * 2.f - 1.f;

    vout.UV = pos;
    vout.Position = mul(Sky.ViewProj, float4(pos, 1.f));
    vout.Position.z = vout.Position.w - 1e-6f;

    return vout;
}