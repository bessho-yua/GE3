#include "DebugCamera.h"

#include <Windows.h>

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#include <dinput.h>

namespace
{
	// 単位行列を作る
	Matrix4x4 MakeIdentityMatrix()
	{
		Matrix4x4 result{};

		result.m[0][0] = 1.0f;
		result.m[1][1] = 1.0f;
		result.m[2][2] = 1.0f;
		result.m[3][3] = 1.0f;

		return result;
	}

	// ベクトルを行列で回転させる
	// 平行移動成分は使用しない
	Vector3 TransformNormal(
		const Vector3& vector,
		const Matrix4x4& matrix)
	{
		Vector3 result{};

		result.x =
			vector.x * matrix.m[0][0] +
			vector.y * matrix.m[1][0] +
			vector.z * matrix.m[2][0];

		result.y =
			vector.x * matrix.m[0][1] +
			vector.y * matrix.m[1][1] +
			vector.z * matrix.m[2][1];

		result.z =
			vector.x * matrix.m[0][2] +
			vector.y * matrix.m[1][2] +
			vector.z * matrix.m[2][2];

		return result;
	}
}

void DebugCamera::Initialize()
{
	// 累積回転行列を単位行列にする
	matRot_ = MakeIdentityMatrix();

	// カメラのワールド行列
	Matrix4x4 worldMatrix =
		Affine::Multiply(
			matRot_,
			Affine::MakeTranslateMatrix(translation_));

	// ワールド行列の逆行列をビュー行列にする
	viewMatrix_ = Affine::Inverse(worldMatrix);
}

void DebugCamera::Update(const std::uint8_t* key)
{
	const float kMoveSpeed = 0.2f;
	const float kRotateSpeed = 0.02f;

	// 今回のフレームで加える回転量
	float rotateX = 0.0f;
	float rotateY = 0.0f;
	float rotateZ = 0.0f;

	// 上下回転
	if (key[DIK_UP]) {
		rotateX -= kRotateSpeed;
	}

	if (key[DIK_DOWN]) {
		rotateX += kRotateSpeed;
	}

	// 左右回転
	if (key[DIK_LEFT]) {
		rotateY -= kRotateSpeed;
	}

	if (key[DIK_RIGHT]) {
		rotateY += kRotateSpeed;
	}

	// Z軸回転
	if (key[DIK_Z]) {
		rotateZ -= kRotateSpeed;
	}

	if (key[DIK_X]) {
		rotateZ += kRotateSpeed;
	}

	// 今回追加する回転行列
	Matrix4x4 matRotDelta = MakeIdentityMatrix();

	matRotDelta =
		Affine::Multiply(
			matRotDelta,
			Affine::MakeRotateXMatrix(rotateX));

	matRotDelta =
		Affine::Multiply(
			matRotDelta,
			Affine::MakeRotateYMatrix(rotateY));

	matRotDelta =
		Affine::Multiply(
			matRotDelta,
			Affine::MakeRotateZMatrix(rotateZ));

	// 今回の回転を累積回転行列へ合成
	matRot_ =
		Affine::Multiply(
			matRotDelta,
			matRot_);

	// ローカル空間での移動量
	Vector3 move = { 0.0f, 0.0f, 0.0f };

	// 前進・後退
	if (key[DIK_W]) {
		move.z += kMoveSpeed;
	}

	if (key[DIK_S]) {
		move.z -= kMoveSpeed;
	}

	// 左右移動
	if (key[DIK_A]) {
		move.x -= kMoveSpeed;
	}

	if (key[DIK_D]) {
		move.x += kMoveSpeed;
	}

	// 上下移動
	if (key[DIK_E]) {
		move.y += kMoveSpeed;
	}

	if (key[DIK_Q]) {
		move.y -= kMoveSpeed;
	}

	// カメラの向きに合わせて移動ベクトルを回転
	move = TransformNormal(move, matRot_);

	// 座標へ移動量を加える
	translation_.x += move.x;
	translation_.y += move.y;
	translation_.z += move.z;

	// 回転行列と平行移動行列から
	// カメラのワールド行列を計算
	Matrix4x4 worldMatrix =
		Affine::Multiply(
			matRot_,
			Affine::MakeTranslateMatrix(translation_));

	// ワールド行列の逆行列をビュー行列にする
	viewMatrix_ = Affine::Inverse(worldMatrix);
}