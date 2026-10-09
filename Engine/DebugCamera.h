#pragma once

#include <cstdint>
#include "affine.h"

class DebugCamera
{
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update(const std::uint8_t* key);

	/// <summary>
	/// ビュー行列を取得
	/// </summary>
	const Matrix4x4& GetViewMatrix() const {
		return viewMatrix_;
	}

private:
	// ローカル座標
	Vector3 translation_ = { 0.0f, 0.0f, -50.0f };

	// 累積回転行列
	Matrix4x4 matRot_{};

	// ビュー行列
	Matrix4x4 viewMatrix_{};
};