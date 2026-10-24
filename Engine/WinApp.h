#pragma once

class WinApp
{
public://静的メンバ関数
	static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WWPARAM wparam, LPARAM lparam);


public://メンバ関数

	//初期化
	void Initialize();
	//更新
	void Update();


};

