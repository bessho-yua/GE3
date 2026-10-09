#include <Windows.h>
#include<cstdint>
#include<string>
#include <format>
#include <filesystem>
#include<fstream>
#include<chrono>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>
#include<dbghelp.h>
#include<strsafe.h>
#include<dxgidebug.h>
#include <dxcapi.h>
#include"affine.h"
#include "DebugCamera.h"
#include"externals/DirectXTex/DirectXTex.h"
#include <sstream>
#include<wrl.h>
#include<xaudio2.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <cstring>
#include <vector>
#include <cmath>
#include <cstring>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam
);
#endif

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"xaudio2.lib")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

struct Vector4 {
	float x, y, z, w;
};

struct Vector2 {
	float x, y;
};

struct Matrix3x3 {
	float m[3][3];
};

struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};



struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct Material {
	Vector4 color;
	int32_t enbleLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

enum class MultipleModelType {
	kSphere,
	kPlane,
};

constexpr const char* kMultipleModelNames[] = {
	"Sphere",
	"Plane",
};

struct MultipleModelObject {
	MultipleModelType type = MultipleModelType::kSphere;

	Transform transform{
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource;
	TransformationMatrix* wvpData = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource;
	Material* materialData = nullptr;
};

struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};

struct MaterialData {
	std::string textureFilePath;
};

struct ModelData {
	std::vector<VertexData> vertices;
	MaterialData material;
};

//チャンクヘッダ
struct ChunkHeader {
	char id[4];
	int32_t size;
};

// RIFFヘッダチャンク
struct RiffHeader {
	ChunkHeader chunk;
	char type[4];
};

//FNTチャンク
struct FormatChunk {
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

//音声データ
struct SoundData {
	//波形フォーマット
	WAVEFORMATEX wfex;
	//バッファの先頭アドレス
	BYTE* pBuffer;
	//バッファのサイズ
	unsigned int bufferSize;
};

enum class SceneType {
	kObjectSprite,
	kSphere,
	kMultipleModels,
	kUtahTeapot,
};

enum class BlendMode {
	kNormal,
	kAdd,
	kSubtract
};

constexpr const char* kSceneNames[] = {
	"Object & Sprite Scene",
	"Sphere Scene",
	"Multiple Models Scene",
	"Utah Teapot Scene",
};

constexpr const char* blendModeNames[] = {
	"Normal",
	"Add",
	"Subtract"
};

//ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
#endif

	//メッセージに応じてゲームの固有の処理を行う
	switch (msg) {
		//ウィンドウが破棄された
	case WM_DESTROY:
		//OSに対して、アプリの終了を伝える
		PostQuitMessage(0);
		return 0;
	}
	//標準のメッセージ処理を行う
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//Log関数
void Log(std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA(message.c_str());
}
//ConvertString
std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

IDxcBlob* CompileShader(
	// CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	// Compilerに使用するProfile
	const wchar_t* profile,
	// 初期化で生成したものを3つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler,
	std::ostream& logStream)
{
	// これからシェーダーをコンパイルする旨をログに出す
	Log(logStream, ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));

	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);

	// 読めなかったら止める
	assert(SUCCEEDED(hr));

	// 読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知
	LPCWSTR arguments[] = {
		filePath.c_str(), // コンパイル対象のhlslファイル名
		L"-E", L"main", // エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile, // ShaderProfileの設定
		L"-Zi", L"-Qembed_debug", // デバッグ用の情報を埋め込む
		L"-Od", // 最適化を外しておく
		L"-Zpr", // メモリレイアウトは行優先
	};

	// 実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer, // 読み込んだファイル
		arguments, // コンパイルオプション
		_countof(arguments), // コンパイルオプションの数
		includeHandler, // includeが含まれた諸々
		IID_PPV_ARGS(&shaderResult) // コンパイル結果
	);

	// コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert(SUCCEEDED(hr));
	// 警告・エラーが出てたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);

	if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
		Log(logStream, shaderError->GetStringPointer());

		// 警告・エラーダメゼッタイ
		assert(false);
	}
	// コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));

	// 成功したログを出す
	Log(logStream, ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));

	// もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();

	// 実行用のバイナリを返却
	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, size_t sizeInBytes) {
	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // UploadHeapを使う

	// 頂点リソースの設定
	D3D12_RESOURCE_DESC vertexResourceDesc{};
	vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexResourceDesc.Width = sizeInBytes;
	vertexResourceDesc.Height = 1;
	vertexResourceDesc.DepthOrArraySize = 1;
	vertexResourceDesc.MipLevels = 1;
	vertexResourceDesc.SampleDesc.Count = 1;
	vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際に頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertexResource)
	);
	assert(SUCCEEDED(hr));

	return vertexResource;
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(const Microsoft::WRL::ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible
		? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
		: D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(
		&descriptorHeapDesc,
		IID_PPV_ARGS(&descriptorHeap)
	);
	assert(SUCCEEDED(hr));

	return descriptorHeap;
}
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}
//単位行列
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; i++) {
		result.m[i][i] = 1.0f;
	}

	return result;
}
//string->wstring
std::wstring ConvertString(const std::string& str);
//wstring->string
std::string ConvertString(const std::wstring& str);

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
	SYSTEMTIME time;
	GetLocalTime(&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory(L"./Dumps", nullptr);
	StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
	DWORD processId = GetCurrentProcessId();
	DWORD threadId = GetCurrentThreadId();
	MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
	minidumpInformation.ThreadId = threadId;
	minidumpInformation.ExceptionPointers = exception;
	minidumpInformation.ClientPointers = TRUE;
	MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
	return EXCEPTION_EXECUTE_HANDLER;
}

//texture読み込み関数
DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	OutputDebugStringA(filePath.c_str());
	OutputDebugStringA("\n");

	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// ミップマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	// ミップマップ付きのデータを返す
	return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata) {
	// metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);                           // Textureの幅
	resourceDesc.Height = UINT(metadata.height);                         // Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);                 // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);          // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;                               // TextureのFormat
	resourceDesc.SampleDesc.Count = 1;                                   // サンプリングカウント。1固定。
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension); // Textureの次元数。普段使っているのは2次元
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; // 細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置
	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_GENERIC_READ, // 初回のResourceState。Textureは基本読むだけ
		nullptr, // Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

void UploadTextureData(const Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages)
{
	// Meta情報を取得
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	// 全MipMapについて
	for (size_t mipLevel = 0; mipLevel < metadata.mipLevels; ++mipLevel) {
		// MipMapLevelを指定して各Imageを取得
		const DirectX::Image* img = mipImages.GetImage(mipLevel, 0, 0);

		// Textureに転送
		HRESULT hr = texture->WriteToSubresource(
			UINT(mipLevel),
			nullptr,              // 全領域へコピー
			img->pixels,           // 元データアドレス
			UINT(img->rowPitch),   // 1ラインサイズ
			UINT(img->slicePitch)  // 1枚サイズ
		);

		assert(SUCCEEDED(hr));
	}
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, int32_t width, int32_t height) {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;                         // Textureの幅
	resourceDesc.Height = height;                       // Textureの高さ
	resourceDesc.MipLevels = 1;                         // mipmapの数
	resourceDesc.DepthOrArraySize = 1;                  // 奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;                  // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DepthStencilとして使う通知

	// 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // VRAM上に作る
	// 深度値のクリア設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f; // 1.0f（最大値）でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // フォーマット。Resourceと合わせる
	// Resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし。
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値を書き込む状態にしておく
		&depthClearValue, // Clear最適値
		IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result = {};

	result.m[0][0] = 2.0f / (right - left);
	result.m[0][1] = 0.0f;
	result.m[0][2] = 0.0f;
	result.m[0][3] = 0.0f;

	result.m[1][0] = 0.0f;
	result.m[1][1] = 2 / (top - bottom);
	result.m[1][2] = 0.0f;
	result.m[1][3] = 0.0f;

	result.m[2][0] = 0.0f;
	result.m[2][1] = 0.0f;
	result.m[2][2] = 1 / (farClip - nearClip);
	result.m[2][3] = 0.0f;

	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1.0f;

	return result;

}

MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
	MaterialData materialData;
	std::string line;
	std::ifstream file(directoryPath + "/" + filename);
	assert(file.is_open());
	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;

		//idenifierに応じた処理
		if (identifier == "map_Kd") {
			std::string textureFilename;
			s >> textureFilename;
			//連結してファイルパスにする
			materialData.textureFilePath = directoryPath + "/" + textureFilename;
		}
	}
	return materialData;
}

ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename) {

	ModelData modelData;
	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;
	std::string line;

	std::ifstream file(directoryPath + "/" + filename);//ファイルを開く
	assert(file.is_open());//開けないとき止める

	while (std::getline(file, line)) {
		std::string identifier;
		std::istringstream s(line);
		s >> identifier;//戦闘の識別子を読む
		if (identifier == "v") {
			Vector4 position;
			s >> position.x >> position.y >> position.z;
			position.w = 1.0f;
			positions.push_back(position);
		}
		else if (identifier == "vt") {
			Vector2 texcoord;
			s >> texcoord.x >> texcoord.y;
			texcoords.push_back(texcoord);
		}
		else if (identifier == "vn") {
			Vector3 normal;
			s >> normal.x >> normal.y >> normal.z;
			normals.push_back(normal);
		}
		else if (identifier == "f") {
			VertexData triangle[3];
			//面は三角形限定。その他は未対応
			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
				std::string vertexDefinition;
				s >> vertexDefinition;
				//頂点の要素絵のIndexは「位置/UV/法線」で格納されているので、分解してIndexを取得する
				std::istringstream v(vertexDefinition);
				uint32_t elementIndices[3];
				for (int32_t element = 0; element < 3; ++element) {
					std::string index;
					std::getline(v, index, '/');//区切りでインデックスを読んでいく
					elementIndices[element] = std::stoi(index);
				}
				//要素へのIndexから、実際の用のの値を取得して、頂点を構築する
				Vector4 position = positions[elementIndices[0] - 1];
				Vector2 texcoord = texcoords[elementIndices[1] - 1];
				Vector3 normal = normals[elementIndices[2] - 1];
				position.x *= -1.0f;
				normal.x *= -1.0f;
				texcoord.y = 1.0f - texcoord.y;
				texcoord.x = 1.0f - texcoord.x;
				VertexData vertex = { position,texcoord,normal };
				modelData.vertices.push_back(vertex);
				triangle[faceVertex] = { position,texcoord,normal };
			}
			//頂点を逆順に登録することで、周り順を逆にする
			modelData.vertices.push_back(triangle[2]);
			modelData.vertices.push_back(triangle[1]);
			modelData.vertices.push_back(triangle[0]);
		}
		else if (identifier == "mtllib") {
			//materialTemplateLibraryファイルの名前を取得する
			std::string materialFilename;
			s >> materialFilename;
			//基本的にobjファイルと同一階層にmtlは存在させるので、ディレクトリ名とファイル名を渡す
			modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
		}
	}
	return modelData;
}

struct D3DResourceLeakChecker {
	~D3DResourceLeakChecker() {
#ifdef _DEBUG
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;

		if (SUCCEEDED(
			DXGIGetDebugInterface1(
				0,
				IID_PPV_ARGS(&debug)))) {

			debug->ReportLiveObjects(
				DXGI_DEBUG_ALL,
				DXGI_DEBUG_RLO_ALL);

			debug->ReportLiveObjects(
				DXGI_DEBUG_APP,
				DXGI_DEBUG_RLO_ALL);

			debug->ReportLiveObjects(
				DXGI_DEBUG_D3D12,
				DXGI_DEBUG_RLO_ALL);
		}
#endif
	}
};

SoundData SoundLoadWave(const char* filename) {

	//ファイル入力ストリームのインスタンス
	std::ifstream file;
	//.wavファイルをバイナリモードで開く
	file.open(filename, std::ios_base::binary);
	//ファイルオープン失敗を検出する
	assert(file.is_open());
	//RIFFヘッダーの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));
	//ファイルがRIFFかチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
		assert(0);
	}
	//タイプがWAVEかチェック
	if (strncmp(riff.type, "WAVE", 4) != 0) {
		assert(0);
	}
	//Formatチャンクの読み込み
	FormatChunk format = {};
	//チャンクヘッダーの確認
	file.read((char*)&format, sizeof(ChunkHeader));
	if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
		assert(0);
	}
	//チャンク本体の読み込み
	assert(format.chunk.size <= sizeof(format.fmt));
	file.read((char*)&format.fmt, format.chunk.size);
	//Dataチャンクの読み込み
	ChunkHeader data;
	file.read((char*)&data, sizeof(data));
	//JUNKチャンクを検出した場合
	if (strncmp(data.id, "JUNK", 4) == 0) {
		//読み取り位置をJUNKチャンクの終わりまで進める
		file.seekg(data.size, std::ios_base::cur);
		//再読み込み
		file.read((char*)&data, sizeof(data));
	}
	if (strncmp(data.id, "data", 4) != 0) {
		assert(0);
	}
	//Dataチャンクのデータ部(波形データ)の読み込み
	char* pBuffer = new char[data.size];
	file.read(pBuffer, data.size);

	//Waveファイルを閉じる
	file.close();

	//returnするための音声データ
	SoundData soundData = {};
	soundData.wfex = format.fmt;
	soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
	soundData.bufferSize = data.size;

	return soundData;
}

//音声データ開放
void SoundUnload(SoundData* soundData) {
	//バッファのメモリを解放
	delete[] soundData->pBuffer;

	soundData->pBuffer = 0;
	soundData->bufferSize = 0;
	soundData->wfex = {};
}

//音声再生
void SoundPlayWave(IXAudio2* xAudio2, const SoundData& soundData) {

	HRESULT hr;

	//波形フォーマットをもとにSourceVoiceの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	hr = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(hr));

	//再生する波形データの再生
	XAUDIO2_BUFFER buf{};
	buf.pAudioData = soundData.pBuffer;
	buf.AudioBytes = soundData.bufferSize;
	buf.Flags = XAUDIO2_END_OF_STREAM;

	//波形データの再生
	hr = pSourceVoice->SubmitSourceBuffer(&buf);
	hr = pSourceVoice->Start();
}

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

#ifdef _DEBUG
	// 必ずDirectX関連のComPtrより先に宣言する
	D3DResourceLeakChecker leakChecker;
#endif

	//
	//WindowsAPI初期化処理
	//

	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	SetUnhandledExceptionFilter(ExportDump);



	std::filesystem::create_directory("logs");
	//現在時刻を取得(UCT時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	//ログファイルの名前にコンマ何秒はいらないので削って秒にする
	std::chrono::time_point < std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	//日本時間(PCの設定時間)に変換
	std::chrono::zoned_time localTime{ std::chrono::current_zone(),nowSeconds };
	//formatを使って年月日_時分秒の文字列に変換
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	//ファイルを作って書き込み準備
	std::ofstream logStream(logFilePath);


	WNDCLASS wc{};

	wc.lpfnWndProc = WindowProc;

	wc.lpszClassName = L"CG2WindowClass";

	wc.hInstance = GetModuleHandle(nullptr);

	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClass(&wc);

	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウの生成
	HWND hwnd = CreateWindow(
		wc.lpszClassName,
		L"CG2",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr);

	//
	//DirectX初期化処理
	//

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	//DXGIファクトリーの生成
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory = nullptr;
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter = nullptr;
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {

			Log(logStream, ConvertString(std::format(L"USE Adapter:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);

	Microsoft::WRL::ComPtr<ID3D12Device> device = nullptr;

	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };

	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device));

		if (SUCCEEDED(hr)) {
			Log(logStream, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}
	assert(device != nullptr);
	Log(logStream, "Complete create D3D12Device\n");
#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		//やばいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		//警告時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		infoQueue->PushStorageFilter(&filter);
	}
#endif

	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	//コマンドキューを生成
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));

	assert(SUCCEEDED(hr));

	//コマンドアロケータを生成
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	//コマンドリストを生成
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr,
		IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	//スワップチェーンを生成
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;
	swapChainDesc.Height = kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	//コマンドキュー、ウィンドウハンドル、設定を渡して生成
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// RTV用
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap =
		CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	// DSV用
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap =
		CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// SRV用
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap =
		CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

	//SwapChainからResourceを引っ張ってくる
	Microsoft::WRL::ComPtr<ID3D12Resource> swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	// DepthStencilTextureをウィンドウのサイズで作成
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource =
		CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);

	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	// DSVHeapの先頭にDSVをつくる
	device->CreateDepthStencilView(
		depthStencilResource.Get(),
		&dsvDesc,
		dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	//RTVの設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

#ifdef USE_IMGUI
	// ImGuiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(
		device.Get(),
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap.Get(),
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->Build();
#endif

	//ディスクリプタの先頭を取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	//RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	//1つ目
	rtvHandles[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0].Get(), &rtvDesc, rtvHandles[0]);
	//2つ目のディスクリプタハンドルを得る
	rtvHandles[1].ptr = rtvHandles[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//2つ目
	device->CreateRenderTargetView(swapChainResources[1].Get(), &rtvDesc, rtvHandles[1]);

	//ウィンドウを表示する
	ShowWindow(hwnd, SW_SHOW);

	//初期値0でFenceを作る
	Microsoft::WRL::ComPtr<ID3D12Fence> fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));
	//FenceのSignalを待つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent != nullptr);

	// dxcCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;

	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));

	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	// includeに対応するための設定
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));

	//
	//描画初期化処理
	//

	//DirectInputの初期化
	IDirectInput8* directInput = nullptr;
	hr = DirectInput8Create(wc.hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(hr));
	//キーボードデバイスの生成
	IDirectInputDevice8* keyboard = nullptr;
	hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(hr));
	//入力データ形式のセット
	hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));
	//排他制御レベルのセット
	hr = keyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// DescriptorRange作成
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	//RootParameter作成
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // Tableで利用する数
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;
	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);


	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX; // ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0; // レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // PixelShaderで使う

	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);

	if (FAILED(hr)) {
		Log(logStream, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = device->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)
	);
	assert(SUCCEEDED(hr));

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};

	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;


	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//
	//BlendModeの元
	//

	//NormalDescblendの設定
	D3D12_BLEND_DESC NormalDescblend{};
	//すべての色要素を書き込む
	NormalDescblend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	NormalDescblend.RenderTarget[0].BlendEnable = TRUE;
	NormalDescblend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	NormalDescblend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	NormalDescblend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	NormalDescblend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	NormalDescblend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	NormalDescblend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	//AddDescblendの設定
	D3D12_BLEND_DESC AddDescblend{};
	//すべての色要素を書き込む
	AddDescblend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	AddDescblend.RenderTarget[0].BlendEnable = TRUE;
	AddDescblend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	AddDescblend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	AddDescblend.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	AddDescblend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	AddDescblend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	AddDescblend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	//SubtractDescblendの設定
	D3D12_BLEND_DESC SubtractDescblend{};
	//すべての色要素を書き込む
	SubtractDescblend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	SubtractDescblend.RenderTarget[0].BlendEnable = TRUE;
	SubtractDescblend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	SubtractDescblend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
	SubtractDescblend.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
	SubtractDescblend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	SubtractDescblend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	SubtractDescblend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;


	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, includeHandler, logStream);
	assert(pixelShaderBlob != nullptr);

	//
	//グラフィックパイプライン
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature.Get(); // RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc; // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() }; // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() }; // PixelShader

	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc; // RasterizerState
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;


	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//Normal用PSOを作る
	graphicsPipelineStateDesc.BlendState = NormalDescblend; // BlendState
	// 実際に生成(Normal)
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineStateNormal = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateNormal));
	assert(SUCCEEDED(hr));

	// Add用PSOを作る
	graphicsPipelineStateDesc.BlendState = AddDescblend;
	// 実際に生成(Add)
	Microsoft::WRL::ComPtr<ID3D12PipelineState>graphicsPipelineStateAdd = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateAdd));
	assert(SUCCEEDED(hr));

	// Subtract用PSOを作る
	graphicsPipelineStateDesc.BlendState = SubtractDescblend;
	// 実際に生成(Subtract)
	Microsoft::WRL::ComPtr<ID3D12PipelineState>graphicsPipelineStateSubtract = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateSubtract));
	assert(SUCCEEDED(hr));



	// plane.objを読み込む
	ModelData modelData = LoadObjFile("resources/plane", "plane.obj");

	assert(!modelData.vertices.empty());

	// OBJ用頂点リソース
	Microsoft::WRL::ComPtr<ID3D12Resource>
		vertexResourceObject =
		CreateBufferResource(
			device,
			sizeof(VertexData) *
			modelData.vertices.size()
		);

	// OBJ用頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW
		vertexBufferViewObject{};

	vertexBufferViewObject.BufferLocation =
		vertexResourceObject->GetGPUVirtualAddress();

	vertexBufferViewObject.SizeInBytes =
		static_cast<UINT>(
			sizeof(VertexData) *
			modelData.vertices.size()
			);

	vertexBufferViewObject.StrideInBytes =
		sizeof(VertexData);

	// OBJ頂点を書き込む
	VertexData* vertexDataObject = nullptr;

	vertexResourceObject->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(
			&vertexDataObject
			)
	);

	std::memcpy(
		vertexDataObject,
		modelData.vertices.data(),
		sizeof(VertexData) *
		modelData.vertices.size()
	);

	ModelData teapotModelData =
		LoadObjFile("resources", "teapot.obj");

	assert(!teapotModelData.vertices.empty());

	Microsoft::WRL::ComPtr<ID3D12Resource>
		vertexResourceTeapot =
		CreateBufferResource(
			device,
			sizeof(VertexData) *
			teapotModelData.vertices.size());

	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewTeapot{};

	vertexBufferViewTeapot.BufferLocation =
		vertexResourceTeapot->GetGPUVirtualAddress();

	vertexBufferViewTeapot.SizeInBytes =
		static_cast<UINT>(
			sizeof(VertexData) *
			teapotModelData.vertices.size());

	vertexBufferViewTeapot.StrideInBytes =
		sizeof(VertexData);

	VertexData* vertexDataTeapot = nullptr;

	vertexResourceTeapot->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&vertexDataTeapot));

	std::memcpy(
		vertexDataTeapot,
		teapotModelData.vertices.data(),
		sizeof(VertexData) *
		teapotModelData.vertices.size());

	const uint32_t kSubdivision = 16;
	const uint32_t kSphereVertexCount = kSubdivision * kSubdivision * 6;

	//Resource作成の関数
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource =
		CreateBufferResource(device, sizeof(VertexData) * kSphereVertexCount);

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// リソースの先頭のアドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();

	// 使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferView.SizeInBytes = sizeof(VertexData) * kSphereVertexCount;


	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);
	// Sprite用の頂点リソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 4);
	// Sprite用の頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	// リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点4つ分
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 4;
	// 1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData);
	// Sprite用の頂点データを書き込む
	VertexData* vertexDataSprite = nullptr;

	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	//音系
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice;
	//XAudioエンジンのインスタンスを生成
	hr = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	//mスターボイスを生成
	hr = xAudio2->CreateMasteringVoice(&masterVoice);
	SoundData soundData1 = SoundLoadWave("resources/Alarm01.wav");
	//音声再生
	SoundPlayWave(xAudio2.Get(), soundData1);

	//Index用
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResourceSprite = CreateBufferResource(device, sizeof(uint32_t) * 6);
	//IndexのView
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	//リソースの先頭アドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress();
	//使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof(uint32_t) * 6;
	//インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
	//インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0; indexDataSprite[1] = 1; indexDataSprite[2] = 2;
	indexDataSprite[3] = 1; indexDataSprite[4] = 3; indexDataSprite[5] = 2;

	// 1枚目の三角形
	// 左下
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[0].normal = { 0.0f,0.0f,-1.0f };
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	// 左上
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].normal = { 0.0f, 0.0f, -1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	// 右下
	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[2].normal = { 0.0f, 0.0f, -1.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	// 左上
	vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[3].normal = { 0.0f, 0.0f, -1.0f };
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };

	// Sprite用のTransformationMatrix用のリソースを作る
	// Sprite用のTransformationMatrix用リソース
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResourceSprite = CreateBufferResource(device, sizeof(TransformationMatrix));

	// 書き込み用ポインタ
	TransformationMatrix* transformationMatrixDataSprite = nullptr;

	// 書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));

	// 初期値
	transformationMatrixDataSprite->WVP = MakeIdentity4x4();
	transformationMatrixDataSprite->World = MakeIdentity4x4();

	// CPUで動かす用のTransformを作る
	Transform transformSprite{
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f }
	};

	VertexData* vertexData = nullptr;

	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));

	//球のために必要

	const float pi = 3.14159265358979323846f;

	// 経度方向の1分割の角度
	const float kLonEvery = 2.0f * pi / float(kSubdivision);

	// 緯度方向の1分割の角度
	const float kLatEvery = pi / float(kSubdivision);

	// 緯度方向に分割
	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		float lat = -pi / 2.0f + kLatEvery * float(latIndex);
		float latNext = lat + kLatEvery;

		float v = 1.0f - float(latIndex) / float(kSubdivision);
		float vNext = 1.0f - float(latIndex + 1) / float(kSubdivision);

		// 経度方向に分割
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			float lon = float(lonIndex) * kLonEvery;
			float lonNext = lon + kLonEvery;

			float u = float(lonIndex) / float(kSubdivision);
			float uNext = float(lonIndex + 1) / float(kSubdivision);

			uint32_t startIndex = (latIndex * kSubdivision + lonIndex) * 6;

			// a
			vertexData[startIndex + 0].position = { cosf(lat) * cosf(lon),sinf(lat),cosf(lat) * sinf(lon),1.0f };
			vertexData[startIndex + 0].texcoord = { u, v };

			// b
			vertexData[startIndex + 1].position = { cosf(latNext) * cosf(lon),sinf(latNext),cosf(latNext) * sinf(lon),1.0f };
			vertexData[startIndex + 1].texcoord = { u, vNext };

			// c
			vertexData[startIndex + 2].position = { cosf(lat) * cosf(lonNext),sinf(lat),cosf(lat) * sinf(lonNext),1.0f };
			vertexData[startIndex + 2].texcoord = { uNext, v };

			// c
			vertexData[startIndex + 3].position = { cosf(lat) * cosf(lonNext),sinf(lat),cosf(lat) * sinf(lonNext),1.0f };
			vertexData[startIndex + 3].texcoord = { uNext, v };

			// b
			vertexData[startIndex + 4].position = { cosf(latNext) * cosf(lon),sinf(latNext),cosf(latNext) * sinf(lon),1.0f };
			vertexData[startIndex + 4].texcoord = { u, vNext };

			// d
			vertexData[startIndex + 5].position = { cosf(latNext) * cosf(lonNext),sinf(latNext),cosf(latNext) * sinf(lonNext),1.0f };
			vertexData[startIndex + 5].texcoord = { uNext, vNext };
			for (uint32_t index = 0; index < 6; ++index) {
				vertexData[startIndex + index].normal = {
					vertexData[startIndex + index].position.x,
					vertexData[startIndex + index].position.y,
					vertexData[startIndex + index].position.z
				};
			}
		}
	}

	//ここまで球

	// OBJ用TransformationMatrix
	Microsoft::WRL::ComPtr<ID3D12Resource>
		wvpResourceObject =
		CreateBufferResource(
			device,
			sizeof(TransformationMatrix)
		);

	TransformationMatrix* wvpDataObject =
		nullptr;

	wvpResourceObject->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(
			&wvpDataObject
			)
	);

	wvpDataObject->WVP = MakeIdentity4x4();
	wvpDataObject->World = MakeIdentity4x4();

	// 球体用TransformationMatrix
	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource =
		CreateBufferResource(device, sizeof(TransformationMatrix));

	TransformationMatrix* wvpData = nullptr;

	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&wvpData));

	wvpData->WVP = MakeIdentity4x4();
	wvpData->World = MakeIdentity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource>
		wvpResourceTeapot =
		CreateBufferResource(
			device,
			sizeof(TransformationMatrix));

	TransformationMatrix* wvpDataTeapot = nullptr;

	wvpResourceTeapot->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&wvpDataTeapot));

	wvpDataTeapot->WVP = MakeIdentity4x4();
	wvpDataTeapot->World = MakeIdentity4x4();

	Microsoft::WRL::ComPtr<ID3D12Resource>
		materialResourceTeapot =
		CreateBufferResource(device, sizeof(Material));

	Material* materialDataTeapot = nullptr;

	materialResourceTeapot->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&materialDataTeapot));

	*materialDataTeapot =
		Material{ {1.0f, 1.0f, 1.0f, 1.0f}, 1 };

	materialDataTeapot->uvTransform =
		MakeIdentity4x4();

	//Sprite用のマテリアルリソースを作る
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceSprite = CreateBufferResource(device, sizeof(Material));

	// OBJ用マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceObject = CreateBufferResource(device, sizeof(Material));


	// 球体用マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource = CreateBufferResource(device, sizeof(Material));

	//ライト用のリソースを作る。
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource = CreateBufferResource(device, sizeof(DirectionalLight));

	// マテリアルにデータを書き込む
	Material* materialData = nullptr;

	// マテリアルにデータを書き込む
	Material* materialDataSprite = nullptr;

	// OBJ用マテリアルデータ
	Material* materialDataObject = nullptr;

	//ライトのデータ
	DirectionalLight* directionalLightData = nullptr;

	// 書き込むためのアドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));

	// 書き込むためのアドレスを取得(Lighting用)
	materialResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite));

	materialResourceObject->Map(0, nullptr, reinterpret_cast<void**>(&materialDataObject));

	materialData->uvTransform = MakeIdentity4x4();

	materialDataSprite->uvTransform = MakeIdentity4x4();

	materialDataObject->uvTransform = MakeIdentity4x4();

	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	// 球体の色
	materialData->color = { 1.0f,1.0f,1.0f,1.0f };

	// 球体はLightingを有効にする
	materialData->enbleLighting = 1;

	// Sprite用マテリアル
	*materialDataSprite = Material{ {1.0f, 1.0f, 1.0f, 1.0f}, 0 };

	// OBJ用マテリアル
	*materialDataObject = Material{ {1.0f, 1.0f, 1.0f, 1.0f}, 1 };
	materialDataObject->uvTransform = MakeIdentity4x4();

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = CreateTextureResource(device, metadata);
	UploadTextureData(textureResource, mipImages);

	// 2枚目Textureを読んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource2 = CreateTextureResource(device, metadata2);
	UploadTextureData(textureResource2, mipImages2);

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// metaDataを基にSRVの設定2
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 1);

	// SRVの生成
	device->CreateShaderResourceView(textureResource.Get(), &srvDesc, textureSrvHandleCPU);

	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle(srvDescriptorHeap, descriptorSizeSRV, 2);

	// SRVの生成2
	device->CreateShaderResourceView(textureResource2.Get(), &srvDesc2, textureSrvHandleCPU2);

	// ビューポート
	D3D12_VIEWPORT viewport{};
	// クライアント領域のサイズと一緒にして画面全体に表示
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// シザー矩形
	D3D12_RECT scissorRect{};
	// 基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;
	//Transform変数
	Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	// OBJ用Transform
	Transform transformObject{
		{5.0f, 5.0f, 5.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

	Transform transformTeapot{
	{3.0f, 3.0f, 3.0f},
	{0.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 0.0f}
	};

	// デバッグカメラ
	DebugCamera debugCamera;
	debugCamera.Initialize();
	Transform uvTransformSprite{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f},
	};
	materialDataSprite->enbleLighting = false;

	SceneType currentScene = SceneType::kObjectSprite;

	BlendMode currentBlendMode = BlendMode::kNormal;

	int selectedModelIndex = 0;

	std::vector<MultipleModelObject> multipleModels;

	auto createMultipleModel =
		[&](MultipleModelType type) {

		multipleModels.emplace_back();
		MultipleModelObject& object = multipleModels.back();

		object.type = type;

		if (type == MultipleModelType::kSphere) {
			object.transform.scale = { 3.0f, 3.0f, 3.0f };
			object.transform.translate = { -4.0f, 0.0f, 0.0f };
		}
		else {
			object.transform.scale = { 5.0f, 5.0f, 5.0f };
			object.transform.translate = { 4.0f, 0.0f, 0.0f };
		}

		object.wvpResource =
			CreateBufferResource(
				device,
				sizeof(TransformationMatrix));

		object.wvpResource->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&object.wvpData));

		object.wvpData->WVP = MakeIdentity4x4();
		object.wvpData->World = MakeIdentity4x4();

		object.materialResource =
			CreateBufferResource(
				device,
				sizeof(Material));

		object.materialResource->Map(
			0,
			nullptr,
			reinterpret_cast<void**>(&object.materialData));

		*object.materialData =
			Material{ {1.0f, 1.0f, 1.0f, 1.0f}, 1 };

		object.materialData->uvTransform =
			MakeIdentity4x4();
		};

	// 最初からSphereとPlaneを1個ずつ表示
	createMultipleModel(MultipleModelType::kSphere);
	createMultipleModel(MultipleModelType::kPlane);

	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightData->direction = { 0.0f,-1.0f,0.0f };
	directionalLightData->intensity = 1.0f;

	//
	//ゲームループ
	//

	MSG msg{};

	while (msg.message != WM_QUIT) {
		//ウィンドウメッセージ処理
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			//ゲームの処理

#ifdef USE_IMGUI
			// ImGuiにフレームが始まることを伝える
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// シーン切り替え用UI
			ImGui::Begin("Scene");

			// Comboはint型を使用するので一度変換
			int currentSceneIndex =
				static_cast<int>(currentScene);

			// Comboはint型を使用するので一度変換

			int BlendModeIndex = static_cast<int>(currentBlendMode);

			if (ImGui::Combo(
				"Current Scene",
				&currentSceneIndex,
				kSceneNames,
				static_cast<int>(_countof(kSceneNames)))) {

				// 選択された番号をSceneTypeに戻す
				currentScene =
					static_cast<SceneType>(currentSceneIndex);
			}

			ImGui::End();

			ImGui::Begin("Blend");

			if (ImGui::Combo(
				"BlendMode",
				&BlendModeIndex,
				blendModeNames,
				static_cast<int>(_countof(blendModeNames)))) {

				//intで選択された数をBlendModeに戻す
				currentBlendMode = static_cast<BlendMode>(BlendModeIndex);
			}

			ImGui::End();

			// 開発用UI
			if (currentScene == SceneType::kObjectSprite) {
				ImGui::Begin("Object & Sprite Settings");

				if (ImGui::CollapsingHeader(
					"Object",
					ImGuiTreeNodeFlags_DefaultOpen)) {

					ImGui::DragFloat3(
						"Object Translate",
						&transformObject.translate.x,
						0.1f);
					ImGui::DragFloat3(
						"Object Rotate",
						&transformObject.rotate.x,
						0.01f);
					ImGui::DragFloat3(
						"Object Scale",
						&transformObject.scale.x,
						0.01f);
					ImGui::ColorEdit4(
						"Object Color",
						&materialDataObject->color.x);
				}

				if (ImGui::CollapsingHeader(
					"Sprite",
					ImGuiTreeNodeFlags_DefaultOpen)) {

					ImGui::DragFloat3(
						"Sprite Translate",
						&transformSprite.translate.x,
						1.0f);
					ImGui::DragFloat3(
						"Sprite Rotate",
						&transformSprite.rotate.x,
						0.01f);
					ImGui::DragFloat3(
						"Sprite Scale",
						&transformSprite.scale.x,
						0.01f);

					ImGui::DragFloat2(
						"UV Translate",
						&uvTransformSprite.translate.x,
						0.01f,
						-10.0f,
						10.0f);
					ImGui::DragFloat2(
						"UV Scale",
						&uvTransformSprite.scale.x,
						0.01f,
						-10.0f,
						10.0f);
					ImGui::SliderAngle(
						"UV Rotate",
						&uvTransformSprite.rotate.z);

					ImGui::ColorEdit4(
						"Sprite Color",
						&materialDataSprite->color.x);
				}

				if (ImGui::CollapsingHeader("Light")) {
					ImGui::ColorEdit4(
						"Light Color",
						&directionalLightData->color.x);
					ImGui::DragFloat3(
						"Light Direction",
						&directionalLightData->direction.x,
						0.01f,
						-1.0f,
						1.0f);
					ImGui::SliderFloat(
						"Light Intensity",
						&directionalLightData->intensity,
						0.0f,
						5.0f);
				}

				ImGui::End();
			}
			else if (currentScene == SceneType::kSphere) {
				ImGui::Begin("Sphere Settings");
				if (ImGui::CollapsingHeader(
					"Object",
					ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::DragFloat3(
						"Sphere Translate",
						&transform.translate.x,
						0.1f);
					ImGui::DragFloat3(
						"Sphere Rotate",
						&transform.rotate.x,
						0.01f);
					ImGui::DragFloat3(
						"Sphere Scale",
						&transform.scale.x,
						0.01f);
					ImGui::ColorEdit4(
						"Sphere Color",
						&materialData->color.x);
				}
				if (ImGui::CollapsingHeader(
					"Light",
					ImGuiTreeNodeFlags_DefaultOpen)) {
					ImGui::ColorEdit4(
						"Light Color",
						&directionalLightData->color.x);
					ImGui::DragFloat3(
						"Light Direction",
						&directionalLightData->direction.x,
						0.01f,
						-1.0f,
						1.0f);
					ImGui::SliderFloat(
						"Light Intensity",
						&directionalLightData->intensity,
						0.0f,
						6.0f);
				}

				ImGui::End();
			}
			else if (currentScene == SceneType::kMultipleModels) {
				ImGui::Begin("Multiple Models Settings");

				ImGui::Combo(
					"Model",
					&selectedModelIndex,
					kMultipleModelNames,
					static_cast<int>(_countof(kMultipleModelNames)));

				if (ImGui::Button("Create")) {
					createMultipleModel(
						static_cast<MultipleModelType>(
							selectedModelIndex));
				}

				int deleteIndex = -1;

				for (size_t i = 0; i < multipleModels.size(); ++i) {
					MultipleModelObject& object = multipleModels[i];

					ImGui::PushID(static_cast<int>(i));

					const char* modelName =
						kMultipleModelNames[
							static_cast<int>(object.type)];

					if (ImGui::CollapsingHeader(
						modelName,
						ImGuiTreeNodeFlags_DefaultOpen)) {

						ImGui::DragFloat3(
							"Translate",
							&object.transform.translate.x,
							0.1f);

						ImGui::DragFloat3(
							"Rotate",
							&object.transform.rotate.x,
							0.01f);

						ImGui::DragFloat3(
							"Scale",
							&object.transform.scale.x,
							0.01f);

						ImGui::ColorEdit4(
							"Color",
							&object.materialData->color.x);

						if (ImGui::Button("Delete")) {
							deleteIndex = static_cast<int>(i);
						}
					}

					ImGui::PopID();
				}

				if (deleteIndex >= 0) {
					multipleModels.erase(
						multipleModels.begin() + deleteIndex);
				}

				ImGui::End();
			}
			else if (currentScene == SceneType::kUtahTeapot) {
				ImGui::Begin("Utah Teapot Settings");

				ImGui::DragFloat3(
					"Teapot Translate",
					&transformTeapot.translate.x,
					0.1f);

				ImGui::DragFloat3(
					"Teapot Rotate",
					&transformTeapot.rotate.x,
					0.01f);

				ImGui::DragFloat3(
					"Teapot Scale",
					&transformTeapot.scale.x,
					0.01f);

				ImGui::ColorEdit4(
					"Teapot Color",
					&materialDataTeapot->color.x);

				if (ImGui::CollapsingHeader("Light")) {
					ImGui::ColorEdit4(
						"Light Color",
						&directionalLightData->color.x);

					ImGui::DragFloat3(
						"Light Direction",
						&directionalLightData->direction.x,
						0.01f,
						-1.0f,
						1.0f);

					ImGui::SliderFloat(
						"Light Intensity",
						&directionalLightData->intensity,
						0.0f,
						5.0f);
				}

				ImGui::End();
			}
#endif

			//DirectX毎フレーム処理

			//これから書き込むバックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
			//TransitionBaffierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			//今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			//Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			//バリアを張る対象のリソース。現在のバッファにし対してして行う
			barrier.Transition.pResource =
				swapChainResources[backBufferIndex].Get();
			//偏移前(現在)のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			//偏移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			//TransitionBarrierを張る
			commandList->ResourceBarrier(1, &barrier);
			// 描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

			commandList->OMSetRenderTargets(1, &rtvHandles[backBufferIndex], false, &dsvHandle);

			// 指定した色で画面全体をクリア
			float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };
			commandList->ClearRenderTargetView(rtvHandles[backBufferIndex], clearColor, 0, nullptr);

			// 指定した深度で画面全体をクリアする
			commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// 描画用のDescriptorHeapを設定
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeaps[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps(1, descriptorHeaps->GetAddressOf());

			commandList->RSSetViewports(1, &viewport); // Viewportを設定
			commandList->RSSetScissorRects(1, &scissorRect); // Scissorを設定
			// RootSignatureを設定。PSOに設定しているけど別途設定が必要
			commandList->SetGraphicsRootSignature(rootSignature.Get());

			switch (currentBlendMode) {
			case BlendMode::kNormal:
				commandList->SetPipelineState(
					graphicsPipelineStateNormal.Get()
				);
				break;

			case BlendMode::kAdd:
				commandList->SetPipelineState(
					graphicsPipelineStateAdd.Get()
				);
				break;
				
			case BlendMode::kSubtract:
				commandList->SetPipelineState(
					graphicsPipelineStateSubtract.Get()
				);
				break;
			}
			// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// キーボード情報の取得開始
			keyboard->Acquire();

			// 全キーの入力状態を取得
			BYTE key[256] = {};
			keyboard->GetDeviceState(sizeof(key), key);

			// どちらのシーンにも3Dオブジェクトがあるのでカメラを更新
			debugCamera.Update(key);

			// Sprite用のWorldViewProjectionMatrixを作る
			Matrix4x4 worldMatrixSprite = Affine::MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);

			// Sprite用のカメラは原点なので単位行列
			Matrix4x4 viewMatrixSprite = MakeIdentity4x4();

			// Sprite用の正射影行列
			Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);

			// Sprite用のWVP行列
			Matrix4x4 worldViewProjectionMatrixSprite = Affine::Multiply(worldMatrixSprite, Affine::Multiply(viewMatrixSprite, projectionMatrixSprite));

			// Sprite用TransformationMatrixへ書き込む
			transformationMatrixDataSprite->WVP =
				worldViewProjectionMatrixSprite;

			transformationMatrixDataSprite->World =
				worldMatrixSprite;

			// デバッグカメラからビュー行列を取得
			Matrix4x4 viewMatrix =
				debugCamera.GetViewMatrix();

			// 球体用World行列
			Matrix4x4 worldMatrix =
				Affine::MakeAffineMatrix(
					transform.scale,
					transform.rotate,
					transform.translate);

			// Projection行列
			Matrix4x4 projectionMatrix =
				Affine::MakePerspectiveFovMatrix(
					0.45f,
					float(kClientWidth) / float(kClientHeight),
					0.1f,
					100.0f);

			Matrix4x4 worldMatrixTeapot =
				Affine::MakeAffineMatrix(
					transformTeapot.scale,
					transformTeapot.rotate,
					transformTeapot.translate);

			wvpDataTeapot->WVP =
				Affine::Multiply(
					worldMatrixTeapot,
					Affine::Multiply(
						viewMatrix,
						projectionMatrix));

			wvpDataTeapot->World =
				worldMatrixTeapot;

			for (MultipleModelObject& object : multipleModels) {
				Matrix4x4 objectWorldMatrix =
					Affine::MakeAffineMatrix(
						object.transform.scale,
						object.transform.rotate,
						object.transform.translate);

				object.wvpData->WVP =
					Affine::Multiply(
						objectWorldMatrix,
						Affine::Multiply(
							viewMatrix,
							projectionMatrix));

				object.wvpData->World = objectWorldMatrix;
			}

			// OBJ用World行列
			Matrix4x4 worldMatrixObject =
				Affine::MakeAffineMatrix(
					transformObject.scale,
					transformObject.rotate,
					transformObject.translate);

			// OBJ用WVP行列
			Matrix4x4 worldViewProjectionMatrixObject =
				Affine::Multiply(
					worldMatrixObject,
					Affine::Multiply(
						viewMatrix,
						projectionMatrix));

			// OBJ用TransformationMatrixへ書き込む
			wvpDataObject->WVP = worldViewProjectionMatrixObject;
			wvpDataObject->World = worldMatrixObject;

			// WVP行列
			Matrix4x4 worldViewProjectionMatrix =
				Affine::Multiply(
					worldMatrix,
					Affine::Multiply(
						viewMatrix,
						projectionMatrix));

			// 球体用TransformationMatrixへ書き込む
			wvpData->WVP = worldViewProjectionMatrix;
			wvpData->World = worldMatrix;

			//UVTransform用
			Matrix4x4 uvTransformMatrix = Affine::MakeScaleMatrix(uvTransformSprite.scale);
			uvTransformMatrix = Affine::Multiply(uvTransformMatrix, Affine::MakeRotateZMatrix(uvTransformSprite.rotate.z));
			uvTransformMatrix = Affine::Multiply(uvTransformMatrix, Affine::MakeTranslateMatrix(uvTransformSprite.translate));
			materialDataSprite->uvTransform = uvTransformMatrix;

#ifdef USE_IMGUI
			// ImGuiの内部コマンドを生成する
			ImGui::Render();
#endif

			//グラフィックスコマンド

			// 選択中のシーンだけ描画
			switch (currentScene) {
			case SceneType::kObjectSprite:
				// 1番目のシーン：plane.objを描画
				commandList->IASetVertexBuffers(
					0,
					1,
					&vertexBufferViewObject);
				commandList->SetGraphicsRootConstantBufferView(
					0,
					materialResourceObject->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(
					1,
					wvpResourceObject->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(
					2,
					textureSrvHandleGPU);
				commandList->SetGraphicsRootConstantBufferView(
					3,
					directionalLightResource->GetGPUVirtualAddress());
				commandList->DrawInstanced(
					static_cast<UINT>(modelData.vertices.size()),
					1,
					0,
					0);

				// 1番目のシーン：Spriteを描画
				commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);

				commandList->IASetIndexBuffer(&indexBufferViewSprite);

				commandList->SetGraphicsRootConstantBufferView(0, materialResourceSprite->GetGPUVirtualAddress());

				commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResourceSprite->GetGPUVirtualAddress());

				commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

				commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
				break;

			case SceneType::kSphere:
				// 2番目のシーン：球体を描画
				commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
				commandList->SetGraphicsRootConstantBufferView(
					0,
					materialResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootConstantBufferView(
					1,
					wvpResource->GetGPUVirtualAddress());
				commandList->SetGraphicsRootDescriptorTable(
					2,
					textureSrvHandleGPU);
				commandList->SetGraphicsRootConstantBufferView(
					3,
					directionalLightResource->GetGPUVirtualAddress());
				commandList->DrawInstanced(
					kSphereVertexCount,
					1,
					0,
					0);
				break;
			case SceneType::kMultipleModels:

				for (MultipleModelObject& object : multipleModels) {
					bool isSphere =
						object.type == MultipleModelType::kSphere;

					const D3D12_VERTEX_BUFFER_VIEW* bufferView =
						isSphere
						? &vertexBufferView
						: &vertexBufferViewObject;

					UINT vertexCount =
						isSphere
						? kSphereVertexCount
						: static_cast<UINT>(
							modelData.vertices.size());

					commandList->IASetVertexBuffers(
						0,
						1,
						bufferView);

					commandList->SetGraphicsRootConstantBufferView(
						0,
						object.materialResource->GetGPUVirtualAddress());

					commandList->SetGraphicsRootConstantBufferView(
						1,
						object.wvpResource->GetGPUVirtualAddress());

					commandList->SetGraphicsRootDescriptorTable(
						2,
						textureSrvHandleGPU);

					commandList->SetGraphicsRootConstantBufferView(
						3,
						directionalLightResource->GetGPUVirtualAddress());

					commandList->DrawInstanced(
						vertexCount,
						1,
						0,
						0);
				}

				break;
			case SceneType::kUtahTeapot:

				commandList->IASetVertexBuffers(
					0,
					1,
					&vertexBufferViewTeapot);

				commandList->SetGraphicsRootConstantBufferView(
					0,
					materialResourceTeapot->GetGPUVirtualAddress());

				commandList->SetGraphicsRootConstantBufferView(
					1,
					wvpResourceTeapot->GetGPUVirtualAddress());

				// uvChecker.pngを使用
				commandList->SetGraphicsRootDescriptorTable(
					2,
					textureSrvHandleGPU);

				commandList->SetGraphicsRootConstantBufferView(
					3,
					directionalLightResource->GetGPUVirtualAddress());

				commandList->DrawInstanced(static_cast<UINT>(teapotModelData.vertices.size()), 1, 0, 0);

				break;
			}
#ifdef USE_IMGUI
			// ImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());
#endif

			//画面入れ替え

			//画面に描く処理はすべて終わり、画面に移すので、状態を偏移
			// 今回はRenderTargetからPresentにする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			//TransitionBaffierを張る
			commandList->ResourceBarrier(1, &barrier);
			//コマンドリストの内容を確定させる
			hr = commandList->Close();
			assert(SUCCEEDED(hr));
			//GPUにコマンドリストの実行を行わせる
			Microsoft::WRL::ComPtr<ID3D12CommandList> commandLists[] = { commandList };
			commandQueue->ExecuteCommandLists(1, commandLists->GetAddressOf());
			//GPUとOSに画面の交換を行うように通知する
			swapChain->Present(1, 0);
			//Fenceの値を更新
			fenceValue++;
			//GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal(fence.Get(), fenceValue);
			//Fenceの値が指定したSigna;値にたどり着いてるか確認する
			//GetCompletedValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue() < fenceValue) {
				//指定したSignalにたどりついてないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				//イベント待つ
				WaitForSingleObject(fenceEvent, INFINITE);
			}
			//次のフレーム用のコマンドリストを準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator.Get(), nullptr);
			assert(SUCCEEDED(hr));
		}
	}

	//WindowsAPI後始末

#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif

	//XAudio2解放
	xAudio2.Reset();
	//音声データ解放
	SoundUnload(&soundData1);

	CloseHandle(fenceEvent);

	CloseWindow(hwnd);

	CoUninitialize();

	return 0;
}