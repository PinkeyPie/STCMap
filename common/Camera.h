#pragma once

#include "utils/d3dUtil.h"

class Camera {
public:
	Camera();
	~Camera();

	// Get/Set world camera position.
	DirectX::XMVECTOR GetPosition() const;
	DirectX::XMFLOAT3 GetPosition3f() const;
	void SetPosition(float x, float y, float z);
	void SetPosition(const DirectX::XMFLOAT3& position);

	// Get camera basis vectors.
	DirectX::XMVECTOR GetRight() const;
	DirectX::XMFLOAT3 GetRight3f() const;
	DirectX::XMVECTOR GetUp() const;
	DirectX::XMFLOAT3 GetUp3f() const;
	DirectX::XMVECTOR GetLook() const;
	DirectX::XMFLOAT3 GetLook3f() const;

	// Get frustum properties.
	float GetNearZ() const;
	float GetFarZ() const;
	float GetAspect() const;
	float GetFovY() const;
	float GetFovX() const;

	// Get near and far plane dimensions in view space coordinates.
	float GetNearWindowWidth() const;
	float GetNearWindowHeight() const;
	float GetFarWindowWidth() const;
	float GetFarWindowHeight() const;

	// Set frustrum
	void SetLens(float fovY, float aspect, float zn, float zf);

	// Define camera space via LookAt parameters
	void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
	void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& worldUp);

	// Get View/Proj matrices
	DirectX::XMMATRIX GetView() const;
	DirectX::XMMATRIX GetProj() const;

	DirectX::XMFLOAT4X4 GetView4x4f() const;
	DirectX::XMFLOAT4X4 GetProj4x4f() const;

	// Strafe/Walk the camera a distance d.
	void Strafe(float d);
	void Walk(float d);

	// Rotate the camera
	void Pitch(float angle);
	void Yaw(float angle);
	void Roll(float angle);

	// After modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();

private:
	// Camera coordinate system with coordinates relative to world space
	DirectX::XMFLOAT3 _fPosition = { 0.f, 0.f, 0.f };
	DirectX::XMFLOAT3 _fRight = { 1.f, 0.f, 0.f };
	DirectX::XMFLOAT3 _fUp = { 0.f, 1.f, 0.f };
	DirectX::XMFLOAT3 _fLook = { 0.f, 0.f, 1.f };

	// Cache frustrum properties
	float _fNearZ = 0.f;
	float _fFarZ = 0.f;
	float _fAspect = 0.f;
	float _fFowY = 0.f;
	float _fNearWindowHeight = 0.f;
	float _fFarWindowHeight = 0.f;

	bool _bViewDirty = true;

	// Cache View/Proj matrixes
	DirectX::XMFLOAT4X4 _fViewMatrix = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 _fProjMatrix = MathHelper::Identity4x4();
};
