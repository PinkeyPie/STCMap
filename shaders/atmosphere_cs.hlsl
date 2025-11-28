#include "common/cs.hlsli"
#include "common/camera.hlsli"
#include "common/light_source.hlsli"

#define RS \
"RootFlags(0), " \
"CBV(b0), " \
"CBV(b1), " \
"StaticSampler(s0," \
    "addressU = TEXTURE_ADDRESS_BORDER," \
    "addressV = TEXTURE_ADDRESS_BORDER," \
    "addressW = TEXTURE_ADDRESS_BORDER," \
    "filter = FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT)," \
"DescriptorTable( SRV(t0, numDescriptors = 2, flags = DESCRIPTORS_VOLATILE), UAV(u0, numDescriptors = 1, flags = DESCRIPTORS_VOLATILE) )"

#define BLOCK_SIZE 16

ConstantBuffer<CameraCb> Camera			: register(b0);
ConstantBuffer<DirectionalLightCb> Sun	: register(b1);
Texture2D<float> DepthBuffer				: register(t0);
Texture2D<float> ShadowMap					: register(t1);

RWTexture2D<float4> Output					: register(u0);

SamplerComparisonState ShadowSampler		: register(s0);


// Returns (distanceToIntersection, distanceThroughVolume)
static float2 RaySphereIntersection(float3 origin, float3 direction, float3 center, float radius)
{
	float3 oc = origin - center;
	const float a = 1.f; // dot(d, d)
	float b = 2.f * dot(oc, direction);
	float c = dot(oc, oc) - radius * radius;
	float d = b * b - 4.f * a * c;

	float2 result = float2(-1.f, -1.f);
	if (d > 0.f)
	{
		float s = sqrt(d);
		float distToNear = max(0.f, (-b - s) / (2.f * a));
		float distToFar = (-b + s) / (2.f * a);

		if (distToFar >= 0.f)
		{
			result = float2(distToNear, distToFar - distToNear);
		}
	}
	return result;
}

// Returns (distanceToIntersection, distanceThroughVolume)
static float2 RayBoxIntersection(float3 origin, float3 direction, float3 minCorner, float3 maxCorner)
{
	float3 invDir = 1.f / direction; // This can be Inf (when one direction component is 0) but still works.

	float tx1 = (minCorner.x - origin.x) * invDir.x;
	float tx2 = (maxCorner.x - origin.x) * invDir.x;

	float tmin = min(tx1, tx2);
	float tmax = max(tx1, tx2);

	float ty1 = (minCorner.y - origin.y) * invDir.y;
	float ty2 = (maxCorner.y - origin.y) * invDir.y;

	tmin = max(tmin, min(ty1, ty2));
	tmax = min(tmax, max(ty1, ty2));

	float tz1 = (minCorner.z - origin.z) * invDir.z;
	float tz2 = (maxCorner.z - origin.z) * invDir.z;

	tmin = max(tmin, min(tz1, tz2));
	tmax = min(tmax, max(tz1, tz2));

	float2 result = float2(-1.f, -1.f);
	if (tmax >= 0.f)
	{
		result = float2(max(0.f, tmin), tmax - tmin);
	}
	return result;
}

static float DensityAtPoint(float3 position)
{
	return 50.8f;
}

static float OpticalDepth(float3 position, float3 direction, float dist)
{
	return DensityAtPoint(position) * dist;
}

[RootSignature(RS)]
[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(CsInput cin)
{
	const uint numInScatteringPoints = 30;

	const float3 minCorner = float3(-80.f, 0.f, -80.f);
	const float3 maxCorner = float3(80.f, 50.f, 80.f);

	const float3 waveLengths = float3(700, 530, 440);
	const float scatteringStrength = 1.f;
	// https://www.desmos.com/calculator/64odtmkk9m
	const float3 scatteringCoefficients = pow(10.f / waveLengths, 4) * scatteringStrength; // Rayleigh scattering.

	const float ditherPattern[4][4] = 
	{ 
		{ 0.0f, 0.5f, 0.125f, 0.625f},
		{ 0.75f, 0.22f, 0.875f, 0.375f},
		{ 0.1875f, 0.6875f, 0.0625f, 0.5625},
		{ 0.9375f, 0.4375f, 0.8125f, 0.3125} 
	};


	uint2 texCoord = cin.DispatchThreadId.xy;
	if (texCoord.x >= (uint)Camera.ScreenDims.x || texCoord.y >= (uint)Camera.ScreenDims.y)
	{
		return;
	}

	float depth = DepthBuffer[texCoord];

	float2 screenUV = texCoord * Camera.InvScreenDims;

	float3 P = RestoreWorldSpacePosition(screenUV, depth, Camera.InvViewProj);
	float3 O = Camera.Position.xyz;
	float3 V = P - O;
	float distanceToGeometry = length(V);
	V /= distanceToGeometry;

	float2 hitInfo = RayBoxIntersection(O, V, minCorner, maxCorner);

	float distToVolume = hitInfo.x;
	float distThroughVolume = min(hitInfo.y, distanceToGeometry);

	if (distToVolume > distanceToGeometry || distThroughVolume <= 0.f)
	{
		Output[texCoord] = float4(0.f, 0.f, 0.f, 0.f); // TODO
		return;
	}

	float ditherOffset = ditherPattern[texCoord.x % 4][texCoord.y % 4];

	const float epsilon = 0.00001f;
	distToVolume += epsilon;
	distThroughVolume -= 2.f * epsilon;

	float stepSize = distThroughVolume / (numInScatteringPoints - 1);
	float3 inScatterPoint = O + (distToVolume + ditherOffset * stepSize) * V;
	float3 inScatteredLight = 0.f;

	float3 L = -Sun.Direction;

	for (uint i = 0; i < numInScatteringPoints; ++i)
	{
		float sunRayLength = RayBoxIntersection(inScatterPoint, L, minCorner, maxCorner).y;
		float sunRayOpticalDepth = OpticalDepth(inScatterPoint, L, sunRayLength);
		float viewRayOpticalDepth = OpticalDepth(inScatterPoint, -V, stepSize * i);

		float3 transmittance = exp(-(sunRayOpticalDepth + viewRayOpticalDepth) * scatteringCoefficients);
		float localDensity = DensityAtPoint(inScatterPoint);

		float pixelDepth = dot(Camera.Forward.xyz, inScatterPoint - O);
		float visibility = SampleCascadedShadowMapSimple(Sun.ViewProj, inScatterPoint, 
			ShadowMap, Sun.Viewports,
			ShadowSampler, pixelDepth, Sun.NumShadowCascades,
			Sun.CascadeDistances, Sun.Bias, Sun.BlendDistances);
            
		inScatteredLight += localDensity * transmittance * visibility;
		inScatterPoint += V * stepSize;
	}

	inScatteredLight *= scatteringCoefficients * stepSize * Sun.Radiance;

	Output[texCoord] = float4(inScatteredLight, 0.f);
}

