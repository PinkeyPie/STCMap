// Transforms and colors geometry.

cbuffer cbPerObject : register(b0)
{
	float4x4	gWorldViewProj;
    float4		gPulseColor;
	float 		gTime;
};

struct VertexIn
{
    float4 Color : COLOR;
	float3 PosL  : POSITION;
};

struct VertexOut
{
	float4 PosH  : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
	// vin.PosL.xy += 0.5 * sin(vin.PosL.x) * sin(3.0 * gTime);
	// vin.PosL.z *= 0.6f + 0.4f * sin(2.0f * gTime);

	VertexOut vout;
	// Transform to homogeneous clip space.
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	
	// Just pass vertex color into the pixel shader.
    vout.Color = vin.Color;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    const float pi = 3.1415926535;
	
	// Oscilate a value in [0,1] over time using sine function.
    float s = 0.5 * sin(2 * gTime - 0.25f * pi) + 0.5f;
	
	// Linearly interpolate between pin.Color and gPulseColor based on parameter a
    float4 c = lerp(pin.Color, gPulseColor, s);
	
    return c;
}
