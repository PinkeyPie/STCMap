
struct PsInput
{
	float3 Color : COLOR0;
};

float4 main(PsInput pin) : SV_TARGET
{
	return float4(pin.Color, 1.f);
}