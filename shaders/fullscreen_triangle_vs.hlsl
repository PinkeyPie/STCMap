
struct VsInput
{
    uint vertexId : SV_VertexID;
};

struct VsOutput
{
    float2 uv : TEXCOORDS;
    float4 position : SV_Position;
};

VsOutput main(VsInput vin)
{
    VsOutput vout;
    
    float x = float((vin.vertexId & 1) << 2) - 1.f;
    float y = 1.f - float((vin.vertexId & 2) << 1);
    float u = x * 0.5f + 0.5f;
    float v = 1.f - (y * 0.5f + 0.5f);
    vout.position = float4(x, y, 0.f, 1.f);
    vout.uv = float2(u, v);
    
    return vout;
}
