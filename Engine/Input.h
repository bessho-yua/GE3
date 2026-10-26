#pragma once
#include "WinApp.h"
#include<windows.h>
#include <wrl.h>
#define DIRECTINPUT_VERSION   0x0800
#include <dinput.h>

class Input
{
public:
	//namespace省略
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
	void Initialize(WinApp* winApp);

	void Update();

	bool PushKey(BYTE keyNumber);

	bool TriggerKey(BYTE keyNumber);

private:
	//WindowsAPI
	WinApp* winApp = nullptr;
	//DirectInputの初期化
	ComPtr<IDirectInput8> directInput = nullptr;
	//キーボードデバイスの生成
	ComPtr<IDirectInputDevice8> keyboard;
	// 全キーの入力状態を取得
	BYTE key[256] = {};
	// 全キーの入力状態を取得
	BYTE keyPre[256] = {};

};

