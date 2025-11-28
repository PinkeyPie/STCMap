#include "TrasformationGizmo.h"
#include "../physics/geometry.h"
#include "../physics/bounding_volumes.h"
#include "../directx/DxCommandList.h"
#include "../directx/DxPipeline.h"

#include "flat_simple_rs.hlsli"

enum EGizmoAxis {
	EGizmoAxisX,
	EGizmoAxisY,
	EGizmoAxisZ,
};

static DxMesh mesh;

static union {
	struct {
		SubmeshInfo TranslationSubmesh;
		SubmeshInfo RotationSubmesh;
		SubmeshInfo ScaleSubmesh;
		SubmeshInfo PlaneSubmesh;
	};

	SubmeshInfo submeshes[4];
};

static BoundingCylinder cylinders[3] = {
	BoundingCylinder{ vec3(0.f), vec3(1.f, 0.f, 0.f), 1.f },
	BoundingCylinder{ vec3(0.f), vec3(0.f, 1.f, 0.f), 1.f },
	BoundingCylinder{ vec3(0.f), vec3(0.f, 0.f, 1.f), 1.f },
};

static BoundingTorus tori[3] =
{
	BoundingTorus{ vec3(0.f), vec3(1.f, 0.f, 0.f), 1.f, 1.f },
	BoundingTorus{ vec3(0.f), vec3(0.f, 1.f, 0.f), 1.f, 1.f },
	BoundingTorus{ vec3(0.f), vec3(0.f, 0.f, 1.f), 1.f, 1.f },
};

struct GizmoRectangle {
	vec3 Position;
	vec3 Tangent;
	vec3 Bitangent;
	vec2 Radius;
};

static GizmoRectangle rectangles[] =
{
	GizmoRectangle{ vec3(1.f, 0.f, 1.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 0.f, 1.f), vec2(1.f) },
	GizmoRectangle{ vec3(1.f, 1.f, 0.f), vec3(1.f, 0.f, 0.f), vec3(0.f, 1.f, 0.f), vec2(1.f) },
	GizmoRectangle{ vec3(0.f, 1.f, 1.f), vec3(0.f, 1.f, 0.f), vec3(0.f, 0.f, 1.f), vec2(1.f) },
};

static bool dragging;
static uint32 axisIndex;
static float anchor;
static vec3 anchor3;
static vec4 plane;

static vec3 originalPosition;
static quat originalRotation;
static vec3 originalScale;

static DxPipeline gizmoPipeline;

static Ptr<struct GizmoMaterial> materials[6];

struct GizmoMaterial : MaterialBase {
	vec4 Color;

	static void SetupOpaquePipeline(DxCommandList* cl, const CommonMaterialInfo& info) {
		cl->SetPipelineState(*gizmoPipeline.Pipeline);
		cl->SetGraphicsRootSignature(*gizmoPipeline.RootSignature);

		cl->SetGraphicsDynamicConstantBuffer(2, info.CameraCBV);
	}

	void PrepareForRendering(DxCommandList* cl) {
		cl->SetGraphics32BitConstants(1, Color);
	}
};


void InitializeTransformationGizmos() {
	CpuMesh mesh(EMeshCreationFlagsWithPositions | EMeshCreationFlagsWithNormals);
	float shaftLength = 1.f;
	float headLength = 0.2f;
	float radius = 0.03f;
	float headRadius = 0.065f;
	TranslationSubmesh = mesh.PushArrow(6, radius, headRadius, shaftLength, headLength);
	RotationSubmesh = mesh.PushTorus(6, 64, shaftLength, radius);
	ScaleSubmesh = mesh.PushMace(6, radius, headRadius, shaftLength, headLength);
	PlaneSubmesh = mesh.PushCube(vec3(shaftLength, 0.01f, shaftLength) * 0.2f, false, vec3(shaftLength, 0.f, shaftLength) * 0.35f);
	::mesh = mesh.CreateDxMesh();

	for (uint32 i = 0; i < 3; ++i) {
		cylinders[i].PositionB *= shaftLength + headLength;
		cylinders[i].Radius *= radius * 1.1f;

		tori[i].MajorRadius *= shaftLength;
		tori[i].TubeRadius *= radius * 1.1f;

		rectangles[i].Position *= shaftLength * 0.35f;
		rectangles[i].Radius *= shaftLength * 0.2f;
	}

	{
		auto desc = CREATE_GRAPHICS_PIPELINE
			.InputLayout(inputLayoutPositionNormal)
			.RenderTargets(DxRenderer::overlayFormat, DxRenderer::overlayDepthFormat)
			;

		gizmoPipeline = DxPipelineFactory::Instance()->CreateReloadablePipeline(desc, { "flat_simple_vs", "flat_simple_ps" });

		for (uint32 i = 0; i < 6; ++i) {
			materials[i] = MakePtr<GizmoMaterial>();
		}
	}
}


static uint32 handleTranslation(trs& transform, Ray r, const UserInput& input, ETransformationSpace space, float snapping, float scaling) {
	quat rot = (space == ETransformationGlobal) ? quat::identity : transform.rotation;

	uint32 hoverAxisIndex = -1;
	float minT = FLT_MAX;
	for (uint32 i = 0; i < 3; ++i) {
		float t;
		BoundingCylinder cylinder = { rot * cylinders[i].PositionA * scaling + transform.position, rot * cylinders[i].PositionB * scaling + transform.position, cylinders[i].Radius * scaling };
		if (r.IntersectCylinder(cylinder, t) && t < minT) {
			hoverAxisIndex = i;
			minT = t;
		}

		if (r.IntersectRectangle(rot * rectangles[i].Position * scaling + transform.position, rot * rectangles[i].Tangent, rot * rectangles[i].Bitangent, rectangles[i].Radius * scaling, t) && t < minT) {
			hoverAxisIndex = i + 3;
			minT = t;
		}
	}

	if (input.Mouse.Left.ClickEvent && hoverAxisIndex != -1) {
		dragging = true;
		axisIndex = hoverAxisIndex;

		if (hoverAxisIndex < 3) {
			vec3 candidate0(0.f); candidate0.data[(axisIndex + 1) % 3] = 1.f;
			vec3 candidate1(0.f); candidate1.data[(axisIndex + 2) % 3] = 1.f;

			const vec4 axisPlanes[] = {
				CreatePlane(transform.position, rot * candidate0),
				CreatePlane(transform.position, rot * candidate1),
			};

			float a = abs(dot(r.Direction, axisPlanes[0].xyz));
			float b = abs(dot(r.Direction, axisPlanes[1].xyz));

			plane = (a > b) ? axisPlanes[0] : axisPlanes[1];

			vec3 axis(0.f); axis.data[axisIndex] = 1.f;
			axis = rot * axis;

			float t;
			r.IntersectPlane(plane.xyz, plane.w, t);
			anchor = dot(r.Origin + t * r.Direction - transform.position, axis);
		}
		else {
			plane = CreatePlane(rot * rectangles[axisIndex - 3].Position + transform.position, rot * cross(rectangles[axisIndex - 3].Tangent, rectangles[axisIndex - 3].Bitangent));
			anchor3 = r.Origin + minT * r.Direction - transform.position;
		}
		originalPosition = transform.position;
	}

	if (dragging) {
		float t;
		r.IntersectPlane(plane.xyz, plane.w, t);

		if (axisIndex < 3) {
			vec3 axis(0.f); axis.data[axisIndex] = 1.f;
			axis = rot * axis;

			float amount = (dot(r.Origin + t * r.Direction - originalPosition, axis) - anchor);

			if (snapping > 0.f) {
				amount = round(amount / snapping) * snapping;
			}

			transform.position = originalPosition + amount * axis;
		}
		else {
			vec3 amount = r.Origin + t * r.Direction - originalPosition - anchor3;

			if (snapping > 0.f) {
				vec3 tangent = rot * rectangles[axisIndex - 3].Tangent;
				vec3 bitangent = rot * rectangles[axisIndex - 3].Bitangent;

				float tangentAmount = round(dot(tangent, amount) / snapping) * snapping;
				float bitangentAmount = round(dot(bitangent, amount) / snapping)* snapping;

				amount = tangent * tangentAmount + bitangent * bitangentAmount;
			}

			transform.position = originalPosition + amount;
		}
	}

	return dragging ? axisIndex : hoverAxisIndex;
}

static uint32 HandleRotation(trs& transform, Ray r, const UserInput& input, ETransformationSpace space, float snapping, float scaling) {
	quat rot = (space == ETransformationGlobal) ? quat::identity : transform.rotation;

	uint32 hoverAxisIndex = -1;
	float minT = FLT_MAX;
	for (uint32 i = 0; i < 3; ++i) {
		float t;
		BoundingTorus torus = { rot * tori[i].Position * scaling + transform.position, rot * tori[i].UpAxis, tori[i].MajorRadius * scaling, tori[i].TubeRadius * scaling };
		if (r.IntersectTorus(torus, t) && t < minT) {
			hoverAxisIndex = i;
			minT = t;
		}
	}

	if (input.Mouse.Left.ClickEvent && hoverAxisIndex != -1) {
		dragging = true;
		axisIndex = hoverAxisIndex;

		vec3 planeNormal(0.f); planeNormal.data[axisIndex] = 1.f;
		planeNormal = rot * planeNormal;
		plane = CreatePlane(transform.position, planeNormal);

		float t;
		r.IntersectPlane(plane.xyz, plane.w, t);

		vec3 p = r.Origin + t * r.Direction - transform.position;
		p = conjugate(rot) * p;
		float x = p.data[(axisIndex + 1) % 3];
		float y = p.data[(axisIndex + 2) % 3];

		anchor = atan2(y, x);
		originalRotation = quat(planeNormal, anchor);
	}

	if (dragging) {
		float t;
		r.IntersectPlane(plane.xyz, plane.w, t);

		vec3 p = r.Origin + t * r.Direction - transform.position;
		p = conjugate(rot) * p;
		float x = p.data[(axisIndex + 1) % 3];
		float y = p.data[(axisIndex + 2) % 3];

		float angle = atan2(y, x);

		if (snapping > 0.f) {
			angle = round((angle - anchor) / snapping) * snapping + anchor;
		}

		quat currentRotation(plane.xyz, angle);
		quat deltaRotation = currentRotation * conjugate(originalRotation);
		transform.rotation = normalize(deltaRotation * transform.rotation);
		if (space == ETransformationGlobal) {
			originalRotation = currentRotation;
		}
	}

	return dragging ? axisIndex : hoverAxisIndex;
}

static uint32 HandleScaling(trs& transform, Ray r, const UserInput& input, ETransformationSpace space, float scaling) {
	quat rot = (space == ETransformationGlobal) ? quat::identity : transform.rotation;

	uint32 hoverAxisIndex = -1;
	float minT = FLT_MAX;
	for (uint32 i = 0; i < 3; ++i) {
		float t;
		BoundingCylinder cylinder = { rot * cylinders[i].PositionA * scaling + transform.position, rot * cylinders[i].PositionB * scaling + transform.position, cylinders[i].Radius * scaling };
		if (r.IntersectCylinder(cylinder, t) && t < minT) {
			hoverAxisIndex = i;
			minT = t;
		}
	}

	if (input.Mouse.Left.ClickEvent && hoverAxisIndex != -1) {
		vec3 candidate0(0.f); candidate0.data[(hoverAxisIndex + 1) % 3] = 1.f;
		vec3 candidate1(0.f); candidate1.data[(hoverAxisIndex + 2) % 3] = 1.f;

		const vec4 axisPlanes[] = {
			CreatePlane(transform.position, rot * candidate0),
			CreatePlane(transform.position, rot * candidate1),
		};

		dragging = true;
		axisIndex = hoverAxisIndex;

		float a = abs(dot(r.Direction, axisPlanes[0].xyz));
		float b = abs(dot(r.Direction, axisPlanes[1].xyz));

		plane = (a > b) ? axisPlanes[0] : axisPlanes[1];

		vec3 axis(0.f); axis.data[axisIndex] = 1.f;
		axis = rot * axis;

		float t;
		r.IntersectPlane(plane.xyz, plane.w, t);
		anchor = dot(r.Origin + t * r.Direction - transform.position, axis);
		originalScale = transform.scale;
	}

	if (dragging) {
		float t;
		r.IntersectPlane(plane.xyz, plane.w, t);

		vec3 axis(0.f); axis.data[axisIndex] = 1.f;
		axis = rot * axis;

		vec3 d = (dot(r.Origin + t * r.Direction - transform.position, axis) / anchor) * axis;

		if (space == ETransformationLocal) {
			d = conjugate(rot) * d;
			transform.scale.data[axisIndex] = originalScale.data[axisIndex] * d.data[axisIndex];
		}
		else {
			d = conjugate(transform.rotation) * d;
			axis = conjugate(transform.rotation) * axis;

			d = abs(d);
			axis = abs(axis);

			float factorX = (d.x > 1.f) ? ((d.x - 1.f) * axis.x + 1.f) : (1.f - (1.f - d.x) * axis.x);
			float factorY = (d.y > 1.f) ? ((d.y - 1.f) * axis.y + 1.f) : (1.f - (1.f - d.y) * axis.y);
			float factorZ = (d.z > 1.f) ? ((d.z - 1.f) * axis.z + 1.f) : (1.f - (1.f - d.z) * axis.z);

			transform.scale = originalScale * vec3(factorX, factorY, factorZ);
		}
	}

	return dragging ? axisIndex : hoverAxisIndex;
}

bool ManipulateTransformation(trs& transform, ETransformationType& type, ETransformationSpace& space, const RenderCamera& camera, const UserInput& input, bool allowInput, OverlayRenderPass* overlayRenderPass) {
	if (!input.Mouse.Left.Down) {
		dragging = false;
	}

	uint32 highlightAxis = -1;


	// Scale gizmos based on distance to camera.
	float scaling = length(transform.position - camera.Position) / camera.GetMinProjectionExtent() * 0.1f;

	if (allowInput) {
		if (input.Keyboard['G'].PressEvent) {
			space = (ETransformationSpace)(1 - space);
			dragging = false;
		}
		if (input.Keyboard['Q'].PressEvent) {
			type = ETransformationTypeNone;
			dragging = false;
		}
		if (input.Keyboard['W'].PressEvent) {
			type = ETransformationTypeTranslation;
			dragging = false;
		}
		if (input.Keyboard['E'].PressEvent) {
			type = ETransformationTypeRotation;
			dragging = false;
		}
		if (input.Keyboard['R'].PressEvent) {
			type = ETransformationTypeScale;
			dragging = false;
		}

		if (type == ETransformationTypeNone) {
			return false;
		}

		float snapping = input.Keyboard[EKey_Ctrl].Down ? (type == ETransformationTypeRotation ? deg2rad(45.f) : 0.5f) : 0.f;

		vec3 originalPosition = transform.position;

		Ray r = camera.GenerateWorldSpaceRay(input.Mouse.RelX, input.Mouse.RelY);


		switch (type) {
			case ETransformationTypeTranslation: highlightAxis = handleTranslation(transform, r, input, space, snapping, scaling); break;
			case ETransformationTypeRotation: highlightAxis = HandleRotation(transform, r, input, space, snapping, scaling); break;
			case ETransformationTypeScale: highlightAxis = HandleScaling(transform, r, input, space, scaling); break; // TODO: Snapping for scale.
		}
	}

	if (type == ETransformationTypeNone) {
		return false;
	}


	// Render.

	quat rot = (space == ETransformationGlobal) ? quat::identity : transform.rotation;

	{
		const quat rotations[] = {
			rot * quat(vec3(0.f, 0.f, -1.f), deg2rad(90.f)),
			rot,
			rot * quat(vec3(1.f, 0.f, 0.f), deg2rad(90.f)),
		};

		const vec4 colors[] = {
			vec4(1.f, 0.f, 0.f, 1.f),
			vec4(0.f, 1.f, 0.f, 1.f),
			vec4(0.f, 0.f, 1.f, 1.f),
		};

		for (uint32 i = 0; i < 3; ++i) {
			materials[i]->Color = colors[i] * (highlightAxis == i ? 0.5f : 1.f);

			overlayRenderPass->RenderObject(mesh.VertexBuffer, mesh.IndexBuffer,
				submeshes[type],
				materials[i],
				CreateModelMatrix(transform.position, rotations[i], scaling),
				true
			);
		}
	}

	if (type == ETransformationTypeTranslation) {
		const quat rotations[] = {
			rot,
			rot * quat(vec3(1.f, 0.f, 0.f), deg2rad(-90.f)),
			rot * quat(vec3(0.f, 0.f, 1.f), deg2rad(90.f)),
		};

		const vec4 colors[] = {
			vec4(1.f, 0.f, 1.f, 1.f),
			vec4(1.f, 1.f, 0.f, 1.f),
			vec4(0.f, 1.f, 1.f, 1.f),
		};

		for (uint32 i = 0; i < 3; ++i) {
			materials[i + 3]->Color = colors[i] * (highlightAxis == i + 3 ? 0.5f : 1.f);

			overlayRenderPass->RenderObject(mesh.VertexBuffer, mesh.IndexBuffer,
				PlaneSubmesh,
				materials[i + 3],
				CreateModelMatrix(transform.position, rotations[i], scaling),
				true
			);
		}
	}

	return dragging;
}

