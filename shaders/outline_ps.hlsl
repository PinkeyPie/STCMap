struct PsInput
{
    float4 color : COLOR;
};

float4 main(PsInput pin) : SV_Target
{
    return pin.color;
}