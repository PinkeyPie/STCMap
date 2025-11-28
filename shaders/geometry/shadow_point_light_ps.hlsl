struct PsInput
{
	float ClipDepth		: CLIP_DEPTH;
};

void main(PsInput pin)
{
	clip(pin.ClipDepth);
}
