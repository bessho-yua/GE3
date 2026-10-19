#pragma once
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
	void Initialize(HINSTANCE hInstance,HWND hwnd);

	void Update();

private:

	//キーボードデバイスの生成
	ComPtr<IDirectInputDevice8> keyboard;

};

