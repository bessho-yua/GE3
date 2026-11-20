#include <Windows.h>
#include<cstdint>
#include<string>
#include <format>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<cassert>
#include<dbghelp.h>
#include<strsafe.h>
#include<dxgidebug.h>
#include <dxcapi.h>
#include"externals/DirectXTex/DirectXTex.h"
#include <sstream>
#include<wrl.h>
#define DIRECTINPUT_VERSION 0x0800
#include <cstring>
#include <vector>
#include <filesystem>
#include<fstream>
#include<chrono>
#include <cmath>
#include <cstdint>

#include"affine.h"
#include "DebugCamera.h"
#include "Input.h"
#include "WinApp.h"
#include "DirectXCommon.h"
#include"Logger.h"
#include "D3DResourceLeakChecker.h"


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

//単位行列
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 result = {};
	for (int i = 0; i < 4; i++) {
		result.m[i][i] = 1.0f;
	}

	return result;
}

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

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

#ifdef _DEBUG
	// 必ずDirectX関連のComPtrより先に宣言する
	D3DResourceLeakChecker leakChecker;
#endif

	//
	//WindowsAPI初期化処理
	//

	SetUnhandledExceptionFilter(ExportDump);

	//WinAppのポインタ
	WinApp* winApp = nullptr;

	//WindowsAPIの初期化
	winApp = new WinApp();
	winApp->Initialize();


	//
	//DirectX初期化処理
	//

	//DirectXのポインタ
	DirectXCommon* dxCommon = nullptr;

	//DirectXの初期化
	dxCommon = new DirectXCommon();
	dxCommon->Initialize(winApp);

	//
	//描画初期化処理
	//

	Input* input = nullptr;

	input = new Input();
	input->Initialize(winApp);

	HRESULT hr;

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
		Logger::Log(dxCommon->GetLogStream(), reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// バイナリを元に生成
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
	hr = dxCommon->GetDevice()->CreateRootSignature(
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


	//RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	//裏面(時計回り)を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	//三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon->CompileShader(L"Object3D.VS.hlsl", L"vs_6_0");
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon->CompileShader(L"Object3D.PS.hlsl", L"ps_6_0");
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
	hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineStateNormal));
	assert(SUCCEEDED(hr));

	//=======================
	//頂点関連計算
	//=======================

	//-------------------
	//Object
	//-------------------

	// plane.objを読み込む
	ModelData modelData = LoadObjFile("resources/plane", "plane.obj");

	assert(!modelData.vertices.empty());

	// OBJ用頂点リソース
	Microsoft::WRL::ComPtr<ID3D12Resource>vertexResourceObject =
		dxCommon->CreateBufferResource(sizeof(VertexData) * modelData.vertices.size());

	// OBJ用頂点バッファビュー
	D3D12_VERTEX_BUFFER_VIEW
		vertexBufferViewObject{};

	vertexBufferViewObject.BufferLocation =
		vertexResourceObject->GetGPUVirtualAddress();

	vertexBufferViewObject.SizeInBytes =
		static_cast<UINT>(sizeof(VertexData) * modelData.vertices.size());

	vertexBufferViewObject.StrideInBytes =
		sizeof(VertexData);

	// OBJ頂点を書き込む
	VertexData* vertexDataObject = nullptr;

	vertexResourceObject->Map(
		0, nullptr, reinterpret_cast<void**>(&vertexDataObject));

	std::memcpy(
		vertexDataObject,
		modelData.vertices.data(),
		sizeof(VertexData) *
		modelData.vertices.size()
	);

	// OBJ用TransformationMatrix
	Microsoft::WRL::ComPtr<ID3D12Resource>wvpResourceObject
		= dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

	TransformationMatrix* wvpDataObject = nullptr;

	wvpResourceObject->Map(0, nullptr, reinterpret_cast<void**>(&wvpDataObject));

	wvpDataObject->WVP = MakeIdentity4x4();

	wvpDataObject->World = MakeIdentity4x4();

	// OBJ用マテリアル
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResourceObject =
		dxCommon->CreateBufferResource(sizeof(Material));

	// OBJ用マテリアルデータ
	Material* materialDataObject = nullptr;

	materialResourceObject->
		Map(0, nullptr, reinterpret_cast<void**>(&materialDataObject));

	// OBJ用マテリアル
	*materialDataObject = Material{ { 1.0f, 1.0f, 1.0f, 1.0f }, 1 };

	materialDataObject->uvTransform = MakeIdentity4x4();

	// OBJ用Transform
	Transform transformObject{
		{ 5.0f, 5.0f, 5.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f }
	};

	//ライト用のリソースを作る。
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource =
		dxCommon->CreateBufferResource(sizeof(DirectionalLight));

	//ライトのデータ
	DirectionalLight* directionalLightData = nullptr;

	directionalLightResource->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData));

	//--------------
	//1つ目の画像
	//--------------

	// Textureを読んで転送する
	DirectX::ScratchImage mipImagesPlane = dxCommon->LoadTexture(modelData.material.textureFilePath);
	const DirectX::TexMetadata& metadataPlane = mipImagesPlane.GetMetadata();
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResourcePlane = dxCommon->CreateTextureResource(metadataPlane);
	dxCommon->UploadTextureData(textureResourcePlane, mipImagesPlane);

	// metaDataを基にSRVの設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDescPlane{};
	srvDescPlane.Format = metadataPlane.format;
	srvDescPlane.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDescPlane.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDescPlane.Texture2D.MipLevels = UINT(metadataPlane.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPUPlane = dxCommon->GetSRVCPUDescriptorHandle(1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPUPlane = dxCommon->GetSRVGPUDescriptorHandle(1);

	// SRVの生成
	dxCommon->GetDevice()->CreateShaderResourceView(textureResourcePlane.Get(), &srvDescPlane, textureSrvHandleCPUPlane);

	// デバッグカメラ
	DebugCamera debugCamera;
	debugCamera.Initialize();

	//assert(false && "assertのテスト");

	directionalLightData->color = { 1.0f,1.0f,1.0f,1.0f };
	directionalLightData->direction = { 0.0f,-1.0f,0.0f };
	directionalLightData->intensity = 1.0f;

	//キー用
	/*if (input->PushKey(DIK_0)) {

	}*/


	//
	//ゲームループ
	//

	MSG msg{};

	while (true) {  //ゲームの処理

		//ウィンドウメッセージ処理
		if (winApp->ProcessMessage()) {
			//ゲームループを抜ける
			break;
		}
		
#ifdef USE_IMGUI
		// ImGuiにフレームが始まることを伝える
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// 開発用UI
		ImGui::Begin("Object Settings");

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

#endif

//DirectX毎フレーム処理

//描画前処理

		dxCommon->PreDraw();

		
		// RootSignatureを設定。PSOに設定しているけど別途設定が必要
		dxCommon->GetCommandList()->SetGraphicsRootSignature(rootSignature.Get());
		dxCommon->GetCommandList()->SetPipelineState(graphicsPipelineStateNormal.Get());


		// 形状を設定。PSOに設定しているものとはまた別。同じものを設定すると考えておけば良い
		dxCommon->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		input->Update();

		// どちらのシーンにも3Dオブジェクトがあるのでカメラを更新
		//debugCamera.Update(key);

		// デバッグカメラからビュー行列を取得
		Matrix4x4 viewMatrix =
			debugCamera.GetViewMatrix();


		//==================================
		//画面に表示する大きさにする計算
		//==================================

		// Projection行列
		Matrix4x4 projectionMatrix =
			Affine::MakePerspectiveFovMatrix(
				0.45f,
				float(WinApp::kClientWidth) / float(WinApp::kClientHeight),
				0.1f,
				100.0f);

		//---------------
		//Object
		//---------------

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


#ifdef USE_IMGUI
		// ImGuiの内部コマンドを生成する
		ImGui::Render();
#endif

		//グラフィックスコマンド

		//plane.objを描画
		dxCommon->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewObject);

		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResourceObject->GetGPUVirtualAddress());

		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResourceObject->GetGPUVirtualAddress());

		dxCommon->GetCommandList()->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPUPlane);

		dxCommon->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource->GetGPUVirtualAddress());

		dxCommon->GetCommandList()->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);


#ifdef USE_IMGUI
		// ImGuiの描画コマンドを積む
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());
#endif

//画面入れ替え

		dxCommon->PostDraw();

	}

	//WindowsAPI後始末

#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif

	delete input;

	//WindowsAPIの終了処理
	winApp->Finalize();

	delete winApp;

	delete dxCommon;

	return 0;
}