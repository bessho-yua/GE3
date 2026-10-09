#pragma once

struct Vector3 {
	float x;
	float y;
	float z;
};

struct Matrix4x4 {
	float m[4][4];
};

class Affine {
public:
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
	static Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	static Matrix4x4 MakeRotateXMatrix(float radian);
	static Matrix4x4 MakeRotateYMatrix(float radian);
	static Matrix4x4 MakeRotateZMatrix(float radian);

	static Matrix4x4 MakeAffineMatrix(
		const Vector3& scale,
		const Vector3& rotate,
		const Vector3& translate
	);
	static Matrix4x4 Inverse(const Matrix4x4& m);

	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
};