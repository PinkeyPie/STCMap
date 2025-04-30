#include "Camera.h"

using namespace DirectX;

Camera::Camera() {
	SetLens(0.25f * MathHelper::Pi, 1.0f, 1.0f, 1000.f);
}

Camera::~Camera() = default;

XMVECTOR Camera::GetPosition() const {
	return XMLoadFloat3(&_fPosition);
}

XMFLOAT3 Camera::GetPosition3f() const {
	return _fPosition;
}

void Camera::SetPosition(float x, float y, float z) {
	_fPosition = XMFLOAT3(x, y, z);
	_bViewDirty = true;
}

void Camera::SetPosition(const DirectX::XMFLOAT3& position) {
	_fPosition = position;
	_bViewDirty = true;
}

DirectX::XMVECTOR Camera::GetRight() const {
	return XMLoadFloat3(&_fRight);
}

DirectX::XMFLOAT3 Camera::GetRight3f() const {
	return _fRight;
}

DirectX::XMVECTOR Camera::GetUp() const {
	return XMLoadFloat3(&_fUp);
}

DirectX::XMFLOAT3 Camera::GetUp3f() const {
	return _fUp;
}

DirectX::XMVECTOR Camera::GetLook() const {
	return XMLoadFloat3(&_fLook);
}

DirectX::XMFLOAT3 Camera::GetLook3f() const {
	return _fLook;
}

float Camera::GetNearZ() const {
	return _fNearZ;
}

float Camera::GetFarZ() const {
	return _fFarZ;
}

float Camera::GetAspect() const {
	return _fAspect;
}

float Camera::GetFovY() const {
	return _fFowY;
}

float Camera::GetFovX() const {
	float halfWidth = 0.5f * GetNearWindowWidth();
	return 2.0f * atan(halfWidth / _fNearZ);
}

float Camera::GetNearWindowWidth() const {
	return _fAspect * _fNearWindowHeight;
}

float Camera::GetNearWindowHeight() const {
	return _fNearWindowHeight;
}

float Camera::GetFarWindowWidth() const {
	return _fAspect * _fFarWindowHeight;
}

float Camera::GetFarWindowHeight() const {
	return _fFarWindowHeight;
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf) {
	// Cache properties
	_fFowY = fovY;
	_fAspect = aspect;
	_fNearZ = zn;
	_fFarZ = zf;

	_fNearWindowHeight = 2.0f * _fNearZ * tanf(0.5 * _fFowY);
	_fFarWindowHeight = 2.0f * _fFarZ * tanf(0.f * _fFowY);

	XMMATRIX P = XMMatrixPerspectiveFovLH(_fFowY, _fAspect, _fNearZ, _fFarZ);
	XMStoreFloat4x4(&_fProjMatrix, P);
}

void Camera::LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp) {
	XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
	XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
	XMVECTOR U = XMVector3Cross(L, R);

	XMStoreFloat3(&_fPosition, pos);
	XMStoreFloat3(&_fLook, L);
	XMStoreFloat3(&_fRight, R);
	XMStoreFloat3(&_fUp, U);

	_bViewDirty = true;
}

void Camera::LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& worldUp) {
	XMVECTOR P = XMLoadFloat3(&pos);
	XMVECTOR T = XMLoadFloat3(&target);
	XMVECTOR W = XMLoadFloat3(&worldUp);

	LookAt(P, T, W);
}

DirectX::XMMATRIX Camera::GetView() const {
	return XMLoadFloat4x4(&_fViewMatrix);
}

DirectX::XMMATRIX Camera::GetProj() const {
	return XMLoadFloat4x4(&_fProjMatrix);
}

DirectX::XMFLOAT4X4 Camera::GetView4x4f() const {
	return _fViewMatrix;
}

DirectX::XMFLOAT4X4 Camera::GetProj4x4f() const {
	return _fProjMatrix;
}

void Camera::Strafe(float d) {
	XMVECTOR s = XMVectorReplicate(d);
	XMVECTOR r = XMLoadFloat3(&_fRight);
	XMVECTOR p = XMLoadFloat3(&_fPosition);
	XMStoreFloat3(&_fPosition, XMVectorMultiplyAdd(s, r, p));

	_bViewDirty = true;
}

void Camera::Walk(float d) {
	XMVECTOR w = XMVectorReplicate(d);
	XMVECTOR l = XMLoadFloat3(&_fLook);
	XMVECTOR p = XMLoadFloat3(&_fPosition);
	XMStoreFloat3(&_fPosition, XMVectorMultiplyAdd(w, l, p));

	_bViewDirty = true;
}

void Camera::Pitch(float angle) {
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&_fRight), angle);

	XMStoreFloat3(&_fUp, XMVector3TransformNormal(XMLoadFloat3(&_fUp), R));
	XMStoreFloat3(&_fLook, XMVector3TransformNormal(XMLoadFloat3(&_fLook), R));

	_bViewDirty = true;
}

void Camera::Yaw(float angle) {
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&_fUp), angle);

	XMStoreFloat3(&_fRight, XMVector3TransformNormal(XMLoadFloat3(&_fRight), R));
	XMStoreFloat3(&_fLook, XMVector3TransformNormal(XMLoadFloat3(&_fLook), R));

	_bViewDirty = true;
}

void Camera::Roll(float angle) {
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&_fLook), angle);

	XMStoreFloat3(&_fRight, XMVector3Transform(XMLoadFloat3(&_fRight), R));
	XMStoreFloat3(&_fUp, XMVector3Transform(XMLoadFloat3(&_fUp), R));

	_bViewDirty = true;
}

void Camera::UpdateViewMatrix() {
	if (_bViewDirty) {
		XMVECTOR R = XMLoadFloat3(&_fRight);
		XMVECTOR L = XMLoadFloat3(&_fLook);
		XMVECTOR U = XMLoadFloat3(&_fUp);
		XMVECTOR P = XMLoadFloat3(&_fPosition);

		// In case if basis was broken
		L = XMVector3Normalize(L);
		U = XMVector3Normalize(XMVector3Cross(L, R));
		R = XMVector3Cross(U, L);

		float x = -XMVectorGetX(XMVector3Dot(P, R)); // sin between P and R
		float y = -XMVectorGetX(XMVector3Dot(P, U));
		float z = -XMVectorGetX(XMVector3Dot(P, L));

		XMStoreFloat3(&_fRight, R);
		XMStoreFloat3(&_fUp, U);
		XMStoreFloat3(&_fLook, L);

		_fViewMatrix(0, 0) = _fRight.x;
		_fViewMatrix(1, 0) = _fRight.y;
		_fViewMatrix(2, 0) = _fRight.z;
		_fViewMatrix(3, 0) = x;

		_fViewMatrix(0, 1) = _fUp.x;
		_fViewMatrix(1, 1) = _fUp.y;
		_fViewMatrix(2, 1) = _fUp.z;
		_fViewMatrix(3, 1) = y;

		_fViewMatrix(0, 2) = _fLook.x;
		_fViewMatrix(1, 2) = _fLook.y;
		_fViewMatrix(2, 2) = _fLook.z;
		_fViewMatrix(3, 2) = z;

		_fViewMatrix(0, 3) = 0.f;
		_fViewMatrix(1, 3) = 0.f;
		_fViewMatrix(2, 3) = 0.f;
		_fViewMatrix(3, 3) = 1.f;

		_bViewDirty = false;
	}
}
