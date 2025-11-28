#include "cameraController.h"

void CameraController::CenterCameraOnObject(const BoundingBox &aabb) {
    vec3 center = aabb.GetCenter();
    float radius = length(aabb.GetRadius());

    vec3 offsetDirection = normalize(Camera->Position - center);

    float minExtent = Camera->GetMinProjectionExtent();

    float scaling = radius / minExtent;

    _centeringTime = 0.f;

    _centeringPositionStart = Camera->Position;
    _centeringRotationStart = Camera->Rotation;

    _centeringPositionTarget = center + scaling * offsetDirection;
    _centeringRotationTarget = LookAtQuaternion(-offsetDirection, vec3(0.f, 1.f, 0.f));

    _orbitRadius = scaling;
}

bool CameraController::Update(const UserInput &input, uint32 viewportWidth, uint32 viewportHeight, float dt) {
    const float CAMERA_MOVEMENT_SPEED = 8.f;
	const float CAMERA_SENSITIVITY = 4.f;
	const float CAMERA_CENTERING_TIME = 0.1f;

	Camera->SetViewport(viewportWidth, viewportHeight);

	bool result = false;

	if (_centeringTime >= 0.f) {
		_centeringTime += dt;

		float relativeTime = Min(_centeringTime / CAMERA_CENTERING_TIME, 1.f);
		Camera->Position = lerp(_centeringPositionStart, _centeringPositionTarget, relativeTime);
		Camera->Rotation = slerp(_centeringRotationStart, _centeringRotationTarget, relativeTime);

		if (relativeTime == 1.f) {
			_centeringTime = -1.f;
		}
	}
	else if (input.Mouse.Right.Down) {
		// Fly camera.

		vec3 cameraInputDir = vec3(
			(input.Keyboard['D'].Down ? 1.f : 0.f) + (input.Keyboard['A'].Down ? -1.f : 0.f),
			(input.Keyboard['E'].Down ? 1.f : 0.f) + (input.Keyboard['Q'].Down ? -1.f : 0.f),
			(input.Keyboard['W'].Down ? -1.f : 0.f) + (input.Keyboard['S'].Down ? 1.f : 0.f)
		) * (input.Keyboard[EKey_Shift].Down ? 3.f : 1.f) * (input.Keyboard[EKey_Ctrl].Down ? 0.1f : 1.f) * CAMERA_MOVEMENT_SPEED;

		vec2 turnAngle(0.f, 0.f);
		turnAngle = vec2(-input.Mouse.RelDx, -input.Mouse.RelDy) * CAMERA_SENSITIVITY;

		quat& cameraRotation = Camera->Rotation;
		cameraRotation = quat(vec3(0.f, 1.f, 0.f), turnAngle.x) * cameraRotation;
		cameraRotation = cameraRotation * quat(vec3(1.f, 0.f, 0.f), turnAngle.y);

		Camera->Position += cameraRotation * cameraInputDir * dt;

		result = true;
	}
	else if (input.Keyboard[EKey_Alt].Down) {
		if (input.Mouse.Left.Down) {
			// Orbit camera.

			vec2 turnAngle(0.f, 0.f);
			turnAngle = vec2(-input.Mouse.RelDx, -input.Mouse.RelDy) * CAMERA_SENSITIVITY;

			quat& cameraRotation = Camera->Rotation;

			vec3 center = Camera->Position + cameraRotation * vec3(0.f, 0.f, -_orbitRadius);

			cameraRotation = quat(vec3(0.f, 1.f, 0.f), turnAngle.x) * cameraRotation;
			cameraRotation = cameraRotation * quat(vec3(1.f, 0.f, 0.f), turnAngle.y);

			Camera->Position = center - cameraRotation * vec3(0.f, 0.f, -_orbitRadius);
		}
		else if (input.Mouse.Middle.Down) {
			// Pan camera.

			vec3 cameraInputDir = vec3(
				-input.Mouse.RelDx * Camera->Aspect,
				input.Mouse.RelDy,
				0.f
			) * (input.Keyboard[EKey_Shift].Down ? 3.f : 1.f) * (input.Keyboard[EKey_Ctrl].Down ? 0.1f : 1.f) * 1000.f * CAMERA_MOVEMENT_SPEED;

			Camera->Position += Camera->Rotation * cameraInputDir * dt;
		}

		result = true;
	}

	Camera->UpdateMatrices();

	return result;
}
