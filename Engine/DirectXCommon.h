#pragma once
#include "WinApp.h"

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
	void InitializeDepthBuffer(int32_t width, int32_t height);
	///<summary>
	///デスクリプタヒープを生成する
	///</summary>
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap
	(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
	///<summary>
	///各種デスクリプタヒープの生成
	///</summary>
	void InitializeVariousDescriptorHeap();
	///<summary>
	///レンダーターゲットビューの初期化
	///</summary>
	void InitializeRenderTargetView();
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
	//<summary>
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
	Microsoft::WRL::ComPtr<ID3D12Resource> resource;
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
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2];
};

