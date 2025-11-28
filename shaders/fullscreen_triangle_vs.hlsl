
struct VsInput
{
    uint VertexId : SV_VertexID;
};

struct VsOutput
{
    float2 UV : TEXCOORDS;
    float4 Position : SV_Position;
};

VsOutput main(VsInput vin)
{
    VsOutput vout;
    
    float x = float((vin.VertexId & 1) << 2) - 1.f;
	float y = 1.f - float((vin.VertexId & 2) << 1);
	float u = x * 0.5f + 0.5f;
	float v = y * 0.5f + 0.5f;
	vout.Position = float4(x, -y, 0.f, 1.f);
	vout.UV = float2(u, v);
    
    return vout;
}
