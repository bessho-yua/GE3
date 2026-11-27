#include "affine.h"
#include <cmath>
#include <cassert>

Matrix4x4 Affine::Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 result = {};

	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			result.m[row][column] =
				m1.m[row][0] * m2.m[0][column] +
				m1.m[row][1] * m2.m[1][column] +
				m1.m[row][2] * m2.m[2][column] +
				m1.m[row][3] * m2.m[3][column];
		}
	}

	return result;
}

Matrix4x4 Affine::MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 result = {};

	result.m[0][0] = 1.0f;
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = 1.0f;
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = 1.0f;
	result.m[2][3] = 0.0f;

	result.m[3][0] = translate.x;
	result.m[3][1] = translate.y;
	result.m[3][2] = translate.z;
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 Affine::MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 result = {};

	result.m[0][0] = scale.x;
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = scale.y;
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = scale.z;
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 Affine::MakeRotateXMatrix(float radian) {
	Matrix4x4 result = {};

	result.m[0][0] = 1.0f;
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = cosf(radian);
	result.m[1][2] = sinf(radian);
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = -sinf(radian);
	result.m[2][2] = cosf(radian);
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 Affine::MakeRotateYMatrix(float radian) {
	Matrix4x4 result = {};

	result.m[0][0] = cosf(radian);
	result.m[0][1] = 0.0f;
	result.m[0][2] = -sinf(radian);
	result.m[0][3] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = 1.0f;
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = sinf(radian);
	result.m[2][1] = 0.0f;
	result.m[2][2] = cosf(radian);
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 Affine::MakeRotateZMatrix(float radian) {
	Matrix4x4 result = {};

	result.m[0][0] = cosf(radian);
	result.m[0][1] = sinf(radian);
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = -sinf(radian);
	result.m[1][1] = cosf(radian);
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = 1.0f;
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 Affine::MakeAffineMatrix(
	const Vector3& scale,
	const Vector3& rotate,
	const Vector3& translate
) {
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);

	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);

	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

	Matrix4x4 rotateXYZMatrix = Multiply(rotateXMatrix, Multiply(rotateYMatrix, rotateZMatrix));

	Matrix4x4 result = Multiply(scaleMatrix, Multiply(rotateXYZMatrix, translateMatrix));

	return result;
}

Matrix4x4 Affine::Inverse(const Matrix4x4& m) {
	Matrix4x4 result = {};
	float determinant = (m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3]) +
		(m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1]) +
		(m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2]) -
		(m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1]) -
		(m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3]) -
		(m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2]) -
		(m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3]) -
		(m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1]) -
		(m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2]) +
		(m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1]) +
		(m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3]) +
		(m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2]) +
		(m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3]) +
		(m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1]) +
		(m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2]) -
		(m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1]) -
		(m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3]) -
		(m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2]) -
		(m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0]) -
		(m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0]) -
		(m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0]) +
		(m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0]) +
		(m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0]) +
		(m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0]);
	float determinantRecp = 1.0f / determinant;
	assert(determinant != 0);
	result.m[0][0] =
		((m.m[1][1] * m.m[2][2] * m.m[3][3]) +
			(m.m[1][2] * m.m[2][3] * m.m[3][1]) +
			(m.m[1][3] * m.m[2][1] * m.m[3][2]) -
			(m.m[1][3] * m.m[2][2] * m.m[3][1]) -
			(m.m[1][2] * m.m[2][1] * m.m[3][3]) -
			(m.m[1][1] * m.m[2][3] * m.m[3][2])) * determinantRecp;
	result.m[0][1] =
		(-(m.m[0][1] * m.m[2][2] * m.m[3][3]) -
			(m.m[0][2] * m.m[2][3] * m.m[3][1]) -
			(m.m[0][3] * m.m[2][1] * m.m[3][2]) +
			(m.m[0][3] * m.m[2][2] * m.m[3][1]) +
			(m.m[0][2] * m.m[2][1] * m.m[3][3]) +
			(m.m[0][1] * m.m[2][3] * m.m[3][2])) * determinantRecp;
	result.m[0][2] =
		((m.m[0][1] * m.m[1][2] * m.m[3][3]) +
			(m.m[0][2] * m.m[1][3] * m.m[3][1]) +
			(m.m[0][3] * m.m[1][1] * m.m[3][2]) -
			(m.m[0][3] * m.m[1][2] * m.m[3][1]) -
			(m.m[0][2] * m.m[1][1] * m.m[3][3]) -
			(m.m[0][1] * m.m[1][3] * m.m[3][2])) * determinantRecp;
	result.m[0][3] =
		(-(m.m[0][1] * m.m[1][2] * m.m[2][3]) -
			(m.m[0][2] * m.m[1][3] * m.m[2][1]) -
			(m.m[0][3] * m.m[1][1] * m.m[2][2]) +
			(m.m[0][3] * m.m[1][2] * m.m[2][1]) +
			(m.m[0][2] * m.m[1][1] * m.m[2][3]) +
			(m.m[0][1] * m.m[1][3] * m.m[2][2])) * determinantRecp;
	result.m[1][0] =
		(-(m.m[1][0] * m.m[2][2] * m.m[3][3]) -
			(m.m[1][2] * m.m[2][3] * m.m[3][0]) -
			(m.m[1][3] * m.m[2][0] * m.m[3][2]) +
			(m.m[1][3] * m.m[2][2] * m.m[3][0]) +
			(m.m[1][2] * m.m[2][0] * m.m[3][3]) +
			(m.m[1][0] * m.m[2][3] * m.m[3][2])) * determinantRecp;
	result.m[1][1] =
		((m.m[0][0] * m.m[2][2] * m.m[3][3]) +
			(m.m[0][2] * m.m[2][3] * m.m[3][0]) +
			(m.m[0][3] * m.m[2][0] * m.m[3][2]) -
			(m.m[0][3] * m.m[2][2] * m.m[3][0]) -
			(m.m[0][2] * m.m[2][0] * m.m[3][3]) -
			(m.m[0][0] * m.m[2][3] * m.m[3][2])) * determinantRecp;
	result.m[1][2] =
		(-(m.m[0][0] * m.m[1][2] * m.m[3][3]) -
			(m.m[0][2] * m.m[1][3] * m.m[3][0]) -
			(m.m[0][3] * m.m[1][0] * m.m[3][2]) +
			(m.m[0][3] * m.m[1][2] * m.m[3][0]) +
			(m.m[0][2] * m.m[1][0] * m.m[3][3]) +
			(m.m[0][0] * m.m[1][3] * m.m[3][2])) * determinantRecp;
	result.m[1][3] =
		((m.m[0][0] * m.m[1][2] * m.m[2][3]) +
			(m.m[0][2] * m.m[1][3] * m.m[2][0]) +
			(m.m[0][3] * m.m[1][0] * m.m[2][2]) -
			(m.m[0][3] * m.m[1][2] * m.m[2][0]) -
			(m.m[0][2] * m.m[1][0] * m.m[2][3]) -
			(m.m[0][0] * m.m[1][3] * m.m[2][2])) * determinantRecp;
	result.m[2][0] =
		((m.m[1][0] * m.m[2][1] * m.m[3][3]) +
			(m.m[1][1] * m.m[2][3] * m.m[3][0]) +
			(m.m[1][3] * m.m[2][0] * m.m[3][1]) -
			(m.m[1][3] * m.m[2][1] * m.m[3][0]) -
			(m.m[1][1] * m.m[2][0] * m.m[3][3]) -
			(m.m[1][0] * m.m[2][3] * m.m[3][1])) * determinantRecp;
	result.m[2][1] =
		(-(m.m[0][0] * m.m[2][1] * m.m[3][3]) -
			(m.m[0][1] * m.m[2][3] * m.m[3][0]) -
			(m.m[0][3] * m.m[2][0] * m.m[3][1]) +
			(m.m[0][3] * m.m[2][1] * m.m[3][0]) +
			(m.m[0][1] * m.m[2][0] * m.m[3][3]) +
			(m.m[0][0] * m.m[2][3] * m.m[3][1])) * determinantRecp;
	result.m[2][2] =
		((m.m[0][0] * m.m[1][1] * m.m[3][3]) +
			(m.m[0][1] * m.m[1][3] * m.m[3][0]) +
			(m.m[0][3] * m.m[1][0] * m.m[3][1]) -
			(m.m[0][3] * m.m[1][1] * m.m[3][0]) -
			(m.m[0][1] * m.m[1][0] * m.m[3][3]) -
			(m.m[0][0] * m.m[1][3] * m.m[3][1])) * determinantRecp;
	result.m[2][3] =
		(-(m.m[0][0] * m.m[1][1] * m.m[2][3]) -
			(m.m[0][1] * m.m[1][3] * m.m[2][0]) -
			(m.m[0][3] * m.m[1][0] * m.m[2][1]) +
			(m.m[0][3] * m.m[1][1] * m.m[2][0]) +
			(m.m[0][1] * m.m[1][0] * m.m[2][3]) +
			(m.m[0][0] * m.m[1][3] * m.m[2][1])) * determinantRecp;
	result.m[3][0] =
		(-(m.m[1][0] * m.m[2][1] * m.m[3][2]) -
			(m.m[1][1] * m.m[2][2] * m.m[3][0]) -
			(m.m[1][2] * m.m[2][0] * m.m[3][1]) +
			(m.m[1][2] * m.m[2][1] * m.m[3][0]) +
			(m.m[1][1] * m.m[2][0] * m.m[3][2]) +
			(m.m[1][0] * m.m[2][2] * m.m[3][1])) * determinantRecp;
	result.m[3][1] =
		((m.m[0][0] * m.m[2][1] * m.m[3][2]) +
			(m.m[0][1] * m.m[2][2] * m.m[3][0]) +
			(m.m[0][2] * m.m[2][0] * m.m[3][1]) -
			(m.m[0][2] * m.m[2][1] * m.m[3][0]) -
			(m.m[0][1] * m.m[2][0] * m.m[3][2]) -
			(m.m[0][0] * m.m[2][2] * m.m[3][1])) * determinantRecp;
	result.m[3][2] =
		(-(m.m[0][0] * m.m[1][1] * m.m[3][2]) -
			(m.m[0][1] * m.m[1][2] * m.m[3][0]) -
			(m.m[0][2] * m.m[1][0] * m.m[3][1]) +
			(m.m[0][2] * m.m[1][1] * m.m[3][0]) +
			(m.m[0][1] * m.m[1][0] * m.m[3][2]) +
			(m.m[0][0] * m.m[1][2] * m.m[3][1])) * determinantRecp;
	result.m[3][3] =
		((m.m[0][0] * m.m[1][1] * m.m[2][2]) +
			(m.m[0][1] * m.m[1][2] * m.m[2][0]) +
			(m.m[0][2] * m.m[1][0] * m.m[2][1]) -
			(m.m[0][2] * m.m[1][1] * m.m[2][0]) -
			(m.m[0][1] * m.m[1][0] * m.m[2][2]) -
			(m.m[0][0] * m.m[1][2] * m.m[2][1])) * determinantRecp;
	return result;
}

Matrix4x4 Affine::MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 result = {};

	float cot = 1.0f / tanf(fovY / 2.0f);

	result.m[0][0] = cot / aspectRatio;
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = cot;
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
	result.m[3][3] = 0.0f;

	return result;
}