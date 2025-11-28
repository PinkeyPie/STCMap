#include "../rs/depth_only_rs.hlsli"
#include "../common/camera.hlsli"

ConstantBuffer<DepthOnlyObjectIdCb> Id	: register(b1);
ConstantBuffer<CameraCb> Camera			: register(b2);

struct PsInput
{
	float3 NDC				: NDC;
	float3 PrevFrameNDC		: PREV_FRAME_NDC;
};

struct PsOutput
{
	float2 ScreenVelocity	: SV_Target0;
	uint ObjectId			: SV_Target1;
};

PsOutput main(PsInput pin)
{
	float2 ndc = (pin.NDC.xy / pin.NDC.z) - Camera.Jitter;
	float2 prevNDC = (pin.PrevFrameNDC.xy / pin.PrevFrameNDC.z) - Camera.PrevFrameJitter;

	float2 motion = (prevNDC - ndc) * float2(0.5f, -0.5f);	

	PsOutput pout;
	pout.ScreenVelocity = motion;
	pout.ObjectId = Id.Id;
	return pout;
}