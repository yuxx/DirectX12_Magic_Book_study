#include "DirectXManager.h"

#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <tchar.h>
#include <iostream>
#include <d3dx12.h>

#include "Helpers.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using namespace yuxx::Debug;
using namespace DirectX;

namespace yuxx {
namespace DirectX12 {
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	DirectXManager* self = nullptr;
	if (msg == WM_NCCREATE) {
		CREATESTRUCT* pcs = reinterpret_cast<CREATESTRUCT*>(lparam);
		self = static_cast<DirectXManager*>(pcs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
	}
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
}

DirectXManager::DirectXManager() = default;

DirectXManager::~DirectXManager()
{
	UnregisterClass(m_windowClass.lpszClassName, m_windowClass.hInstance);
}

bool DirectXManager::Initialize(HINSTANCE hInstance, int width, int height)
{
#ifdef _DEBUG
	if (! DirectXManager::EnableDebugLayer()) {
		return false;
	}
#endif // _DEBUG
	if (!MakeWindow(hInstance, width, height)) {
		return false;
	}

#ifdef _DEBUG
	const UINT factoryFlag = DXGI_CREATE_FACTORY_DEBUG;
#else
	const UINT factoryFlag = 0;
#endif

	HRESULT result = CreateDXGIFactory2(factoryFlag, IID_PPV_ARGS(m_dxgiFactory.GetAddressOf()));
	if (FAILED(result)) {
		DebugOutputFormatString("CreateDXGIFactory2 Error : 0x%x\n", result);
		return false;
	}
	if (!SelectAdapter()) {
		DebugOutputFormatString("No suitable adapter found.\n");
		return false;
	}
	if (!InitDirect3DDevice()) {
		DebugOutputFormatString("InitDirect3DDevice failed.\n");
		return false;
	}
	if (!InitCommandAllocatorAndCommandQueue()) {
		DebugOutputFormatString("InitCommandAllocatorAndCommandQueue failed.\n");
		return false;
	}
	if (!InitSwapChain()) {
		DebugOutputFormatString("InitSwapChain failed.\n");
		return false;
	}
	if (!InitRTV()) {
		DebugOutputFormatString("InitRTV failed.\n");
		return false;
	}
	if (!InitFence()) {
		DebugOutputFormatString("InitFence failed.\n");
		return false;
	}

	if (!SetupVertexBuffer()) {
		DebugOutputFormatString("SetupVertexBuffer failed.\n");
		return false;
	}

	if (!SetupShaders()) {
		DebugOutputFormatString("SetupShaders failed.\n");
		return false;
	}

	if (!SetupGraphicsPipeline()) {
		DebugOutputFormatString("SetupGraphicsPipeline failed.\n");
		return false;
	}

	SetupViewportAndScissor(width, height);

	if (!LoadTexture()) {
		DebugOutputFormatString("LoadTexture failed.\n");
		return false;
	}

	if (!MakeShaderResourceView()) {
		DebugOutputFormatString("MakeShaderResourceView failed.\n");
		return false;
	}

	ShowWindow(m_hwnd, SW_SHOW);

	return true;
}

bool DirectXManager::EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	if (FAILED(result)) {
		DebugOutputFormatString("D3D12GetDebugInterface Error : 0x%x\n", result);
		return false;
	}
	DebugOutputFormatString("DebugLayer is enabled.\n");
	debugLayer->EnableDebugLayer();
	debugLayer->Release();
	return true;
}

bool DirectXManager::MakeWindow(HINSTANCE hInstance, int width, int height)
{
	DebugOutputFormatString("Show m_windowClass test.\n");

	m_windowClass.cbSize = sizeof(WNDCLASSEX);
	m_windowClass.lpfnWndProc = WindowProcedure;
	m_windowClass.lpszClassName = _T("DX12Sample");
	m_windowClass.hInstance = hInstance;

	RegisterClassEx(&m_windowClass);

	m_windowRect = { 0, 0, width, height };

	AdjustWindowRect(&m_windowRect, WS_OVERLAPPEDWINDOW, false);

	m_hwnd = CreateWindow(
		m_windowClass.lpszClassName,
		_T("DirectX12テスト"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		m_windowRect.right - m_windowRect.left,
		m_windowRect.bottom - m_windowRect.top,
		nullptr,
		nullptr,
		hInstance,
		this
	);
	if (!m_hwnd) {
		DebugOutputFormatString("CreateWindow Error : 0x%x\n", GetLastError());
		return false;
	}

	return true;
}

bool DirectXManager::SelectAdapter()
{
	for (int i = 0; m_dxgiFactory->EnumAdapters(i, m_adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC adapterDesc{};
		m_adapter->GetDesc(&adapterDesc);
		std::wstring desc = adapterDesc.Description;
		if (desc.find(L"NVIDIA") != std::string::npos)
		{
			DebugOutputFormatString("NVIDIA Video card found!!!!!!!\n");
			return true;
		}
		if (desc.find(L"AMD") != std::string::npos)
		{
			DebugOutputFormatString("AMD Video card found!\n");
			return true;
		}
	}
	return false;
}

bool DirectXManager::InitDirect3DDevice()
{
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	for (const auto fl : featureLevels) {
		if (SUCCEEDED(D3D12CreateDevice(m_adapter.Get(), fl, IID_PPV_ARGS(m_device.GetAddressOf())))) {
			m_feature_level = fl;
			DebugOutputFormatString("Feature level: 0x%x\n", fl);
			return true;
		}
	}
	DebugOutputFormatString("InitDirect3DDevice failed.\n");
	return false;
}

bool DirectXManager::InitCommandAllocatorAndCommandQueue()
{
	auto result = m_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_commandAllocator.GetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommandAllocator Error : 0x%x\n", result);
		return false;
	}
	result = m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(m_commandList.GetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommandList Error : 0x%x\n", result);
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};

	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.NodeMask = 0;
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	result = m_device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(m_commandQueue.GetAddressOf()));
	if (FAILED(result)) {
		DebugOutputFormatString("InitCommandAllocatorAndCommandQueue Error : 0x%x\n", result);
		return false;
	}

	return true;
}

bool DirectXManager::InitSwapChain()
{
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
	swapchainDesc.Width = m_windowRect.right - m_windowRect.left;
	swapchainDesc.Height = m_windowRect.bottom - m_windowRect.top;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;

	// note: バックバッファは伸び縮み可能
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;

	// note: フリップ後は速やかに破棄
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// note: アルファモードの指定は特にない
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// note: m_windowClass <-> fullscreen 切り替え可能
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	auto result = m_dxgiFactory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),
		m_hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		reinterpret_cast<IDXGISwapChain1**>(m_swapChain.GetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateSwapChainForHwnd Error : 0x%x\n", result);
		return false;
	}

	return true;
}

bool DirectXManager::InitRTV()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NodeMask = 0;
	rtvHeapDesc.NumDescriptors = 2;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT result = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf()));
	if (FAILED(result)) {
		DebugOutputFormatString("CreateDescriptorHeap Error : 0x%x\n", result);
		return false;
	}

	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	result = m_swapChain->GetDesc(&swapChainDesc);
	if (FAILED(result)) {
		DebugOutputFormatString("GetDesc Error : 0x%x\n", result);
		return false;
	}
	m_backBuffers.resize(swapChainDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

	// SRGB レンダーターゲットビューを作成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	const size_t rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (size_t i = 0; i < swapChainDesc.BufferCount; ++i) {
		result = m_swapChain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(m_backBuffers[i].GetAddressOf()));
		if (FAILED(result)) {
			DebugOutputFormatString("GetBuffer Error : 0x%x\n", result);
			return false;
		}
		m_device->CreateRenderTargetView(m_backBuffers[i].Get(), &rtvDesc, rtvHandle);
		rtvHandle.ptr += rtvDescriptorSize;
	}

	return true;
}

bool DirectXManager::InitFence()
{
	auto result = m_device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.GetAddressOf()));
	if (FAILED(result)) {
		DebugOutputFormatString("CreateFence Error : 0x%x\n", result);
		return false;
	}

	return true;
}

bool DirectXManager::SetupVertexBuffer()
{
	D3D12_HEAP_PROPERTIES heapProperties{};

	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resourceDescription{};

	resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDescription.Width = sizeof(kVertices); // 頂点情報が入るだけのサイズ
	resourceDescription.Height = 1;
	resourceDescription.DepthOrArraySize = 1;
	resourceDescription.MipLevels = 1;
	resourceDescription.Format = DXGI_FORMAT_UNKNOWN;
	resourceDescription.SampleDesc.Count = 1;
	resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	auto result = m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDescription,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_vertexBuffer.GetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommittedResource Error (for vertices): 0x%x\n", result);
		return false;
	}

	// 設定はバッファーのサイズ以外、頂点バッファーの設定を使いまわしてよい
	resourceDescription.Width = sizeof(kIndices);

	result = m_device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDescription,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_indexBuffer.GetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommittedResource Error (for indices): 0x%x\n", result);
		return false;
	}

	return true;
}

bool DirectXManager::SetupShaders()
{
	Vertex* verticesMap = nullptr;
	HRESULT result = m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&verticesMap));
	if (FAILED(result)) {
		DebugOutputFormatString("Vertex buffer map Error : 0x%x\n", result);
		return false;
	}
	std::copy(std::begin(kVertices), std::end(kVertices), verticesMap);
	m_vertexBuffer->Unmap(0, nullptr);

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = sizeof(kVertices);
	m_vertexBufferView.StrideInBytes = sizeof(kVertices[0]);

	// 作ったバッファーにインデックスデータをバッファーにコピー
	unsigned short* indicesMap = nullptr;
	result = m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indicesMap));
	if (FAILED(result)) {
		DebugOutputFormatString("Index buffer map Error : 0x%x\n", result);
		return false;
	}
	std::copy(std::begin(kIndices), std::end(kIndices), indicesMap);
	m_indexBuffer->Unmap(0, nullptr);

	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_indexBufferView.SizeInBytes = sizeof(kIndices);

	ComPtr<ID3D10Blob> errorBlob;
	result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_vsBlob.ReleaseAndGetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf()
	);
	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			DebugOutputFormatString("Vertex Shader File not found.\n");
			return false;
		}
		if (errorBlob == nullptr) {
			DebugOutputFormatString("D3DCompileFromFile Vertex Shader Error : 0x%08x\n", result);
			return false;
		}
		std::string errorMessage;
		errorMessage.resize(errorBlob->GetBufferSize());
		std::copy_n(
			static_cast<const char*>(errorBlob->GetBufferPointer()),
			errorBlob->GetBufferSize(),
			errorMessage.begin()
		);
		errorMessage += "\n";
		DebugOutputFormatString(
			"D3DCompileFromFile Vertex Shader Error : %s\n",
			errorMessage.c_str()
		);
		return false;
	}

	result = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		m_psBlob.GetAddressOf(),
		errorBlob.ReleaseAndGetAddressOf()
	);
	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			DebugOutputFormatString("Pixel Shader File not found.\n");
			return false;
		}
		if (errorBlob == nullptr) {
			DebugOutputFormatString("D3DCompileFromFile Pixel Shader Error : 0x%08x\n", result);
			return false;
		}
		std::string errorMessage;
		errorMessage.resize(errorBlob->GetBufferSize());
		std::copy_n(
			static_cast<const char*>(errorBlob->GetBufferPointer()),
			errorBlob->GetBufferSize(),
			errorMessage.begin()
		);
		errorMessage += "\n";
		DebugOutputFormatString(
			"D3DCompileFromFile Pixel Shader Error : %s\n",
			errorMessage.c_str()
		);
		return false;
	}
	return true;
}

bool DirectXManager::SetupGraphicsPipeline()
{
	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ // 座標情報
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{ // uv情報
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipeline{};

	graphicsPipeline.VS.pShaderBytecode = m_vsBlob->GetBufferPointer();
	graphicsPipeline.VS.BytecodeLength = m_vsBlob->GetBufferSize();
	graphicsPipeline.PS.pShaderBytecode = m_psBlob->GetBufferPointer();
	graphicsPipeline.PS.BytecodeLength = m_psBlob->GetBufferSize();

	// デフォルトのサンプルマスクを表す定数(0xffffffff)
	graphicsPipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// まだアンチエイリアスは使わないため false
	graphicsPipeline.RasterizerState.AntialiasedLineEnable = false;

	// カリングしない
	graphicsPipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	// 中身を塗りつぶす
	graphicsPipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	// 深度方向のクリッピングは有効に
	graphicsPipeline.RasterizerState.DepthClipEnable = true;

	graphicsPipeline.BlendState.AlphaToCoverageEnable = false;
	graphicsPipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc{};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	graphicsPipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	graphicsPipeline.InputLayout.pInputElementDescs = inputLayout;
	graphicsPipeline.InputLayout.NumElements = _countof(inputLayout);

	// トライアングルストリップのカットなし
	graphicsPipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	// 三角形で構成
	graphicsPipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipeline.NumRenderTargets = 1;
	// 0～1 に正規化された RGBA
	graphicsPipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	// サンプリングは1ピクセルにつき1
	graphicsPipeline.SampleDesc.Count = 1;
	// クオリティは最低
	graphicsPipeline.SampleDesc.Quality = 0;


	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	// 種別はテクスチャ
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	// テクスチャ１つ
	descriptorRange.NumDescriptors = 1;
	// 0版スロットから
	descriptorRange.BaseShaderRegister = 0;
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameter{};
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// ピクセルシェーダーから見えるようにする
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// ディスクリプタレンジのアドレス
	rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;
	// ディスクリプタレンジ数
	rootParameter.DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC samplerDesc{};

	// 線形補間
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	// // 補間しない(ニアレストネイバー法: 最近傍補間)
	// samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	// 横方向の繰り返し
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	// 縦方向の繰り返し
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	// 奥行き方向の繰り返し
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	// リサンプリングしない
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	// ボーダーカラーは使わないので透明黒
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	// ミップマップ最小値
	samplerDesc.MinLOD = 0.0f;
	// ミップマップ最大値
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	// ピクセルシェーダーから見える
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};

	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	rootSignatureDesc.pParameters = &rootParameter;
	rootSignatureDesc.NumParameters = 1;
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ComPtr<ID3DBlob> rootSignatureBlob;
	ComPtr<ID3D10Blob> errorBlob;
	HRESULT result = D3D12SerializeRootSignature(
		// ルートシグネチャ設定
		&rootSignatureDesc,
		// ルートシグネチャバージョン
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		// シェーダーを作ったときと同じ
		rootSignatureBlob.ReleaseAndGetAddressOf(),
		// エラー処理も同じ
		errorBlob.ReleaseAndGetAddressOf()
	);
	if (FAILED(result)) {
		if (errorBlob == nullptr) {
			DebugOutputFormatString("D3D12SerializeRootSignature Error : 0x%x\n", result);
			return false;
		}
		std::string errorMessage;
		errorMessage.resize(errorBlob->GetBufferSize());
		std::copy_n(
			static_cast<const char*>(errorBlob->GetBufferPointer()),
			errorBlob->GetBufferSize(),
			errorMessage.begin()
		);
		errorMessage += "\n";
		DebugOutputFormatString(
			"D3D12SerializeRootSignature Error : 0x%x\n",
			errorMessage.c_str()
		);
		return false;
	}

	result = m_device->CreateRootSignature(
		// nodeMask。0でよい
		0,
		// シェーダーの時と同様
		rootSignatureBlob->GetBufferPointer(),
		// シェーダーの時と同様
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateRootSignature Error : 0x%x\n", result);
		return false;
	}

	graphicsPipeline.pRootSignature = m_rootSignature.Get();

	result = m_device->CreateGraphicsPipelineState(
		&graphicsPipeline,
		IID_PPV_ARGS(m_pipelineState.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateGraphicsPipelineState Error : 0x%x\n", result);
		return false;
	}

	return true;
}

void DirectXManager::SetupViewportAndScissor(unsigned int windowWidth, unsigned int windowHeight)
{
	// 出力先の幅(ピクセル数)
	m_viewport.Width = windowWidth;
	// 出力先の高さ(ピクセル数)
	m_viewport.Height = windowHeight;
	// 出力先の左上X座標
	m_viewport.TopLeftX = 0.0f;
	// 出力先の左上Y座標
	m_viewport.TopLeftY = 0.0f;
	// 深度最大値
	m_viewport.MaxDepth = 1.0f;
	// 深度最小値
	m_viewport.MinDepth = 0.0f;

	// 切り抜き上座標
	m_scissorRect.top = 0;
	// 切り抜き左座標
	m_scissorRect.left = 0;
	// 切り抜き右座標
	m_scissorRect.right = m_scissorRect.left + windowWidth;
	// 切り抜き下座標
	m_scissorRect.bottom = m_scissorRect.top + windowHeight;
}

void DirectXManager::SetupUploadHeap(
	D3D12_HEAP_PROPERTIES& uploadHeapProperties,
	D3D12_RESOURCE_DESC& uploadResourceDescription,
	const Image* image
) {
	// マップ可能にするため、UPLOAD にする
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// アップロード用に使用すること前提なので UNKNOWN でよい
	uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// 単一アダプターのため0
	uploadHeapProperties.CreationNodeMask = 0;
	uploadHeapProperties.VisibleNodeMask = 0;


	// 単なるデータの塊なので UNKNOWN
	uploadResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
	// 単なるバッファーとして指定
	uploadResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;

	// データサイズ
	uploadResourceDescription.Width =
		AlignedSize(image->slicePitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * image->height;
	uploadResourceDescription.Height = 1;
	uploadResourceDescription.DepthOrArraySize = 1;
	uploadResourceDescription.MipLevels = 1;

	// 連続したデータ
	uploadResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	// 特にフラグなし
	uploadResourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

	uploadResourceDescription.SampleDesc = {
		// 通常テクスチャなのでアンチエイリアシングはしない
		1,
		0
	};
}

void DirectXManager::SetupTextureHeap(
	D3D12_HEAP_PROPERTIES& textureHeapProperties,
	D3D12_RESOURCE_DESC& resourceDescription
) {
	// テクスチャ用
	textureHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	textureHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	textureHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// 単一アダプターのため0
	textureHeapProperties.CreationNodeMask = 0;
	textureHeapProperties.VisibleNodeMask = 0;

	// RGBA フォーマット
	resourceDescription.Format = m_metadata.format;
	// 幅
	resourceDescription.Width = m_metadata.width;
	// 高さ
	resourceDescription.Height = m_metadata.height;
	resourceDescription.DepthOrArraySize = m_metadata.arraySize;
	resourceDescription.SampleDesc = {
		// 通常のテクスチャなのでアンチエイリアシングは使わない
		1,
		// クオリティは最低
		0
	};
	resourceDescription.MipLevels = m_metadata.mipLevels;
	resourceDescription.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(m_metadata.dimension);
	// レイアウトは決定しない
	resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	// 特にフラグなし
	resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
}

size_t DirectXManager::AlignedSize(size_t size, size_t alignment)
{
	return size + alignment - size % alignment;
}

void DirectXManager::SetupTextureBufferLocation(
	D3D12_TEXTURE_COPY_LOCATION& srcLocation,
	D3D12_TEXTURE_COPY_LOCATION& dstLocation,
	ID3D12Resource* uploadBuffer,
	const TexMetadata& metadata,
	const Image* image
) const
{
	// コピー元(アップロード側)設定
	srcLocation.pResource = uploadBuffer;
	// フットプリントを指定
	srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	srcLocation.PlacedFootprint.Offset = 0;
	srcLocation.PlacedFootprint.Footprint.Width = metadata.width;
	srcLocation.PlacedFootprint.Footprint.Height = metadata.height;
	srcLocation.PlacedFootprint.Footprint.Depth = metadata.depth;
	srcLocation.PlacedFootprint.Footprint.RowPitch =
		AlignedSize(image->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	srcLocation.PlacedFootprint.Footprint.Format = image->format;

	// コピー先設定
	dstLocation.pResource = m_textureBuffer.Get();
	dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dstLocation.SubresourceIndex = 0;
}

void DirectXManager::SetupTextureResourceBarrier(D3D12_RESOURCE_BARRIER& textureResourceBarrier) const
{
	textureResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	textureResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	textureResourceBarrier.Transition.pResource = m_textureBuffer.Get();
	textureResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	textureResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	textureResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

bool DirectXManager::LoadTexture()
{
	// WIC テクスチャのロード
	ScratchImage scratchImage{};

	HRESULT result = LoadFromWICFile(
		L"img/視力検査の気球.jpg",
		// L"img/ティファ.jpg",
		WIC_FLAGS_NONE,
		&m_metadata,
		scratchImage
	);
	if (FAILED(result)) {
		DebugOutputFormatString("LoadFromWICFile Error : 0x%x\n", result);
		return false;
	}
	// 生データ抽出
	auto image = scratchImage.GetImage(0, 0, 0);

	// 中間バッファーとしてのアップロードヒープ設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	D3D12_RESOURCE_DESC uploadResourceDescription{};
	SetupUploadHeap(uploadHeapProperties, uploadResourceDescription, image);

	// 中間バッファー作成
	ComPtr<ID3D12Resource> uploadBuffer;
	result = m_device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&uploadResourceDescription,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommittedResource Error (for upload texture): 0x%x\n", result);
		return false;
	}

	// テクスチャのためのヒープ設定
	D3D12_HEAP_PROPERTIES textureHeapProperties{};
	D3D12_RESOURCE_DESC resourceDescription{};
	SetupTextureHeap(textureHeapProperties, resourceDescription);

	result = m_device->CreateCommittedResource(
		&textureHeapProperties,
		// 特に指定なし
		D3D12_HEAP_FLAG_NONE,
		&resourceDescription,
		// コピー先
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(m_textureBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommittedResource Error (for texture): 0x%x\n", result);
		return false;
	}

	// image->pixels と同じ型にする
	uint8_t* mapForImage = nullptr;
	result = uploadBuffer->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&mapForImage)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("Upload buffer map Error : 0x%x\n", result);
		return false;
	}
	// ピッチ修正しつつコピー
	uint8_t* sourceAddress = image->pixels;
	size_t rowPitch = AlignedSize(image->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	for (size_t y = 0; y < image->height; ++y) {
		std::copy_n(
			sourceAddress,
			rowPitch,
			mapForImage
		);
		// 1行ごとのつじつまを合わせる
		sourceAddress += image->rowPitch;
		mapForImage += rowPitch;
	}
	uploadBuffer->Unmap(0, nullptr);

	D3D12_TEXTURE_COPY_LOCATION srcLocation{};
	D3D12_TEXTURE_COPY_LOCATION dstLocation{};
	SetupTextureBufferLocation(
		srcLocation,
		dstLocation,
		uploadBuffer.Get(),
		m_metadata,
		image
	);

	m_commandList->CopyTextureRegion(
		&dstLocation,
		0,
		0,
		0,
		&srcLocation,
		nullptr
	);

	D3D12_RESOURCE_BARRIER textureResourceBarrier = {};
	SetupTextureResourceBarrier(textureResourceBarrier);

	m_commandList->ResourceBarrier(1, &textureResourceBarrier);
	result = m_commandList->Close();
	if (FAILED(result)) {
		DebugOutputFormatString("Command list close Error : 0x%x\n", result);
		return false;
	}

	// コマンドリストを実行
	ID3D12CommandList* commandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(1, commandLists);
	
	m_commandQueue->Signal(m_fence.Get(), ++m_fenceValue);
	if (m_fence->GetCompletedValue() != m_fenceValue) {
		auto event = CreateEvent(nullptr, false, false, nullptr);
		m_fence->SetEventOnCompletion(m_fenceValue, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}


	result = m_commandAllocator->Reset();
	if (FAILED(result)) {
		DebugOutputFormatString("Command allocator reset Error : 0x%x\n", result);
		return false;
	}
	result = m_commandList->Reset(m_commandAllocator.Get(), nullptr);
	if (FAILED(result)) {
		DebugOutputFormatString("Command list reset Error : 0x%x\n", result);
		return false;
	}

	return true;
}

bool DirectXManager::MakeShaderResourceView()
{
	D3D12_DESCRIPTOR_HEAP_DESC textureHeapDesc = {};

	// シェーダーから見えるように
	textureHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// マスクは 0
	textureHeapDesc.NodeMask = 0;

	// ビューは今のところ1つだけ
	textureHeapDesc.NumDescriptors = 1;

	// シェーダーリソースビュー用
	textureHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	// 生成
	HRESULT result = m_device->CreateDescriptorHeap(
		&textureHeapDesc,
		IID_PPV_ARGS(m_textureDescriptionHeap.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateDescriptorHeap Error (for texture): 0x%x\n", result);
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	srvDesc.Format = m_metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// 2D テクスチャ
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	// ミップマップは使用しないので1
	srvDesc.Texture2D.MipLevels = 1;

	m_device->CreateShaderResourceView(
		m_textureBuffer.Get(),
		&srvDesc,
		m_textureDescriptionHeap->GetCPUDescriptorHandleForHeapStart()
	);

	return true;
}

bool DirectXManager::Render()
{
	const UINT backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();

	// Note: バリアを設定
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition = {
		m_backBuffers[backBufferIndex].Get(),
		0,
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	};
	m_commandList->ResourceBarrier(1, &barrier);

	// Note: レンダーターゲットの設定
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += backBufferIndex * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, true, nullptr);

	// Note: 画面をクリア
	float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	m_commandList->SetPipelineState(m_pipelineState.Get());

	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	ID3D12DescriptorHeap* heaps[] = { m_textureDescriptionHeap.Get() };
	m_commandList->SetDescriptorHeaps(1, heaps);
	m_commandList->SetGraphicsRootDescriptorTable(
		// ルートパラメーターインデックス
		0,
		// ヒープアドレス
		m_textureDescriptionHeap->GetGPUDescriptorHandleForHeapStart()
	);

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

	m_commandList->IASetIndexBuffer(&m_indexBufferView);

	m_commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);


	// Note: バリアを解除する
	std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
	m_commandList->ResourceBarrier(1, &barrier);

	// Note: コマンドリスト受付を終了
	HRESULT result = m_commandList->Close();
	if (FAILED(result)) {
		DebugOutputFormatString("Command list close Error : 0x%x\n", result);
		return false;
	}

	// Note: コマンドリストを実行
	ID3D12CommandList* commandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(1, commandLists);
	// Note: GPU 完了を待つ
	m_commandQueue->Signal(m_fence.Get(), ++m_fenceValue);

	if (m_fence->GetCompletedValue() != m_fenceValue) {
		auto event = CreateEvent(nullptr, false, false, nullptr);
		m_fence->SetEventOnCompletion(m_fenceValue, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	// Note: クリア
	result = m_commandAllocator->Reset();
	if (FAILED(result)) {
		DebugOutputFormatString("Command allocator reset Error : 0x%x\n", result);
		return false;
	}
	result = m_commandList->Reset(m_commandAllocator.Get(), nullptr);
	if (FAILED(result)) {
		DebugOutputFormatString("Command list reset Error : 0x%x\n", result);
		return false;
	}

	// Note: Flip
	m_swapChain->Present(1, 0);

	return true;
}
}
}
