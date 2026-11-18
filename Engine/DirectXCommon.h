#pragma once
#include "WinApp.h"

#include <Windows.h>
#include <cstdint>
#include <array>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>

class DirectXCommon
{
public:
	///<summary>
	///DirectXの初期化
	///</summary>
	void Initialize(WinApp* winApp);
	///<summary>
	///デバイスの初期化
	///</summary>
	void InitializeDevice();
	///<summary>
	///コマンド関連の初期化
	///</summary>
	void InitializeCommand();
	///<summary>
	///スワップチェーンの生成
	///</summary>
	void InitializeSwapChain();
	///<summary>
	///深度バッファの生成
	///</summary>
	void InitializeDepthBuffer();
	///<summary>
	///各種デスクリプタヒープの生成
	///</summary>
	void InitializeVariousDescriptorHeap();
	///<summary>
	///デスクリプタヒープを生成する
	///</summary>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap
	(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	///<summary>
	///レンダーターゲットビューの初期化
	///</summary>
	void InitializeRenderTargetView();
	///<summary>
	///深度ステンシルビューの初期化
	///</summary>
	void InitializeDepthStencilView();
	///<summary>
	///フェンスの初期化
	///</summary>
	void InitializeFence();
	///<summary>
	///ビューポート矩形の初期化
	///</summary>
	void InitializeViewportRectangle();
	///<summary>
	///シザリング矩形の初期化
	///</summary>
	void InitializeScissorRectangle();
	///<summary>
	///DXCコンパイラの生成
	///</summary>
	void GeneratingDXCCompiler();
#ifdef USE_IMGUI
	///<summary>
	///ImGuiの初期化
	///</summary>
	void InitializeImGui();
#endif
	///<summary>
	///描画前処理
	///</summary>
	void PreDraw();
	///<summary>
	///描画後処理
	///</summary>
	void PostDraw();
	///<summary>
	///SRVの指定番号のCPUデスクリプタハンドルを取得する
	///</summary>
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
	///<summary>
	///SRVの指定番号のGPUデスクリプタハンドルを取得する
	///</summary>
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);
private:

	///<summary>
	///指定番号のCPUデスクリプタハンドルを取得する
	///</summary>
	static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
		const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize, uint32_t index);
	///<summary>
	///指定番号のGPUデスクリプタハンドルを取得する
	///</summary>
	static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
		const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
		uint32_t descriptorSize, uint32_t index);

	//WindowsAPI
	WinApp* winApp = nullptr;

#ifdef _DEBUG
	//デバグコントローラー
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
#endif
	//assert使用
	HRESULT hr;
	//DXGIファクトリ
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory;
	//アダプター
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter;
	//DirectX12デバイス
	Microsoft::WRL::ComPtr<ID3D12Device> device;
	//インフォ列
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
	//コマンドアロケータ
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	//コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
	//コマンド列
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	//スワップチェーン
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
	// 深度バッファーのResourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource;
	//SRVのデスクリプタサイズ
	uint32_t descriptorSizeSRV;
	//RTVのデスクリプタサイズ
	uint32_t descriptorSizeRTV;
	//DSVのデスクリプタサイズ
	uint32_t descriptorSizeDSV;
	//RTVのデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
	//DSVのデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
	//SRVのデスクリプタヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap;
	//スワップチェーンリソース
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,2> swapChainResources;
	//RTVハンドル
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];//RTVを2つ作るのでディスクリプタを2つ用意
	//fence
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	//fenceの大きさ
	uint64_t fenceValue;
	//fenceイベント
	HANDLE fenceEvent;
	//ビューポート
	D3D12_VIEWPORT viewport{};
	// シザー矩形
	D3D12_RECT scissorRect{};
	// DXCユーティリティの生成
	IDxcUtils* dxcUtils;
	// DXCコンパイラの生成
	IDxcCompiler3* dxcCompiler;
	//インクルードハンドラー
	IDxcIncludeHandler* includeHandler;
	//TransitionBaffierの設定
	D3D12_RESOURCE_BARRIER barrier{};
};

