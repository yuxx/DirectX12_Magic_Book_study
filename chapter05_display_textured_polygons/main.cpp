#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <d3dcompiler.h>
#include <DirectXTex.h>

#ifdef _DEBUG
#include <iostream>
#endif // _DEBUG

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using namespace std;
using namespace DirectX;

// @brief コンソール画面にフォーマット付きの文字列を表示
// @param printf 形式の format
// @param 可変長引数
// @remarks この関数はデバッグ用です。デバッグ時にしか動作しません。
void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	vprintf(format, valist);
	va_end(valist);
#endif // _DEBUG
}

// 頂点データ構造体
struct Vertex {
	XMFLOAT3 position; // xyz座標
	XMFLOAT2 uv;       // uv座標
};

struct TexRGBA
{
	unsigned char R, G, B, A;
};


LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY) {
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

constexpr unsigned int window_width = 1280;
constexpr unsigned int window_height = 720;

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	if (FAILED(result)) {
		DebugOutputFormatString("D3D12GetDebugInterface Error : 0x%x\n", result);
		return;
	}
	DebugOutputFormatString("DebugLayer is enabled.\n");
	debugLayer->EnableDebugLayer();
	debugLayer->Release();
}

void InitDirect3DDevice(D3D_FEATURE_LEVEL& detectedFeatureLevel)
{
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	for (const auto fl : featureLevels) {
		if (SUCCEEDED(D3D12CreateDevice(nullptr, fl, IID_PPV_ARGS(&_dev)))) {
			detectedFeatureLevel = fl;
			break;
		}
	}
}

void SetDXGIAdapter(IDXGIAdapter** tmpAdapter)
{
	std::vector<IDXGIAdapter*> adapters;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(*tmpAdapter);
	}

	for (const auto adapter: adapters)
	{
		DXGI_ADAPTER_DESC adapterDesc = {};
		adapter->GetDesc(&adapterDesc);

		std::wstring desc = adapterDesc.Description;

		if (desc.find(L"NVIDIA") != std::string::npos)
		{
			*tmpAdapter = adapter;
			DebugOutputFormatString("NVIDIA Video card found!\n");
			break;
		}
	}
}

bool CreateD3D12CommandListAndAllocator(
	ID3D12CommandAllocator*& commandAllocator,
	ID3D12GraphicsCommandList*& commandList
) {
	auto result = _dev->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&commandAllocator)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommandAllocator Error : 0x%x\n", result);
		return false;
	}
	result = _dev->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator,
		nullptr,
		IID_PPV_ARGS(&commandList)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommandList Error : 0x%x\n", result);
		return false;
	}

	// note: コマンドリストは生成直後にクローズしておく必要がある
	commandList->Close();

	return true;
}

bool CreateD3D12CommandQueue(
	ID3D12CommandQueue*& commandQueue,
	D3D12_COMMAND_QUEUE_DESC& commandQueueDesc
) {
	// note: タイムアウトなし
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	// note: アダプターを1つしか使わない場合は0で問題ない
	commandQueueDesc.NodeMask = 0;
	// note: プライオリティ指定は特になし
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	// note: タイプをコマンドリストと合わせる
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// note: キューを生成
	auto result = _dev->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommandQueue Error : 0x%x\n", result);
		return false;
	}
	return true;
}

bool CreateD3D12SwapChain(
	DXGI_SWAP_CHAIN_DESC1& swapchainDesc,
	ID3D12CommandQueue* commandQueue,
	HWND hwnd
) {
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
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

	// note: window <-> fullscreen 切り替え可能
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	auto result = _dxgiFactory->CreateSwapChainForHwnd(
		commandQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateSwapChainForHwnd Error : 0x%x\n", result);
		return false;
	}

	return true;
}

bool CreateD3D12DescriptorHeap(
	ID3D12DescriptorHeap*& rtvHeap,
	D3D12_DESCRIPTOR_HEAP_DESC& heapDesc
) {
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	auto result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap));
	if (FAILED(result)) {
		DebugOutputFormatString("CreateDescriptorHeap Error : 0x%x\n", result);
		return false;
	}

	return true;
}

bool AssociateDescriptorAndBackBufferOnSwapChain(
	ID3D12DescriptorHeap* rtvHeap,
	std::vector<ID3D12Resource*>& backBuffers
) {
	DXGI_SWAP_CHAIN_DESC swapchainDesc = {};
	auto result = _swapchain->GetDesc(&swapchainDesc);
	if (FAILED(result)) {
		DebugOutputFormatString("GetDesc Error : 0x%x\n", result);
		return false;
	}
	backBuffers.resize(swapchainDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	const size_t rtvDescriptorSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (size_t i = 0; i < swapchainDesc.BufferCount; ++i) {
		result = _swapchain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&backBuffers[i]));
		if (FAILED(result)) {
			DebugOutputFormatString("GetBuffer Error : 0x%x\n", result);
			return false;
		}
		_dev->CreateRenderTargetView(backBuffers[i], nullptr, rtvHandle);
		rtvHandle.ptr += rtvDescriptorSize;
	}

	return true;
}

bool SetupFence(UINT64 &_fenceValue, ID3D12Fence** _fence)
{
	auto result = _dev->CreateFence(_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence));
	if (FAILED(result)) {
		DebugOutputFormatString("CreateFence Error : 0x%x\n", result);
		return false;
	}

	return true;
}

template <size_t N, size_t M>
bool SetupVertexBuffer(
	const Vertex (&vertices)[N],
	ID3D12Resource** vertexBuffer,
	const unsigned short (&indices)[M],
	ID3D12Resource** indexBuffer
) {
	D3D12_HEAP_PROPERTIES heapProperties = {};

	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC resourceDescription = {};

	resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDescription.Width = sizeof(vertices); // 頂点情報が入るだけのサイズ
	resourceDescription.Height = 1;
	resourceDescription.DepthOrArraySize = 1;
	resourceDescription.MipLevels = 1;
	resourceDescription.Format = DXGI_FORMAT_UNKNOWN;
	resourceDescription.SampleDesc.Count = 1;
	resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
	resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	auto result = _dev->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDescription,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(vertexBuffer)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommittedResource Error (for vertices): 0x%x\n", result);
		return false;
	}

	// 設定はバッファーのサイズ以外、頂点バッファーの設定を使いまわしてよい
	resourceDescription.Width = sizeof(indices);

	result = _dev->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDescription,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(indexBuffer)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommittedResource Error (for indices): 0x%x\n", result);
		return false;
	}

	return true;
}

bool ExecuteDirectXProcedure(
	ID3D12DescriptorHeap* rtvHeap,
	ID3D12GraphicsCommandList* commandList,
	ID3D12CommandQueue* commandQueue,
	ID3D12CommandAllocator* commandAllocator,
	ID3D12Fence* fence,
	UINT64& fenceValue,
	std::vector<ID3D12Resource*>& backBuffers,
	ID3D12PipelineState* pipelineState,
	ID3D12RootSignature* rootSignature,
	const D3D12_VIEWPORT& viewport,
	const D3D12_RECT& scissorRect,
	const D3D12_VERTEX_BUFFER_VIEW& vertexBufferView,
	const D3D12_INDEX_BUFFER_VIEW& indexBufferView,
	ID3D12DescriptorHeap* textureDescriptionHeap
) {
	const UINT backBufferIndex = _swapchain->GetCurrentBackBufferIndex();

	// Note: クリア
	HRESULT result = commandAllocator->Reset();
	if (FAILED(result)) {
		DebugOutputFormatString("Command allocator reset Error : 0x%x\n", result);
		return false;
	}
	result = commandList->Reset(commandAllocator, nullptr);
	if (FAILED(result)) {
		DebugOutputFormatString("Command list reset Error : 0x%x\n", result);
		return false;
	}

	// Note: レンダーターゲットの設定
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += backBufferIndex * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// Note: バリアを設定
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition = {
		backBuffers[backBufferIndex],
		0,
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET
	};
	commandList->ResourceBarrier(1, &barrier);

	commandList->OMSetRenderTargets(1, &rtvHandle, true, nullptr);

	// Note: 画面をクリア
	float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	commandList->SetPipelineState(pipelineState);
	commandList->SetGraphicsRootSignature(rootSignature);
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissorRect);

	commandList->SetDescriptorHeaps(1, &textureDescriptionHeap);
	commandList->SetGraphicsRootDescriptorTable(
		// ルートパラメーターインデックス
		0,
		// ヒープアドレス
		textureDescriptionHeap->GetGPUDescriptorHandleForHeapStart()
	);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

	commandList->IASetIndexBuffer(&indexBufferView);

	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);


	// Note: バリアを解除する
	std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
	commandList->ResourceBarrier(1, &barrier);

	// Note: コマンドリスト受付を終了
	result = commandList->Close();
	if (FAILED(result)) {
		DebugOutputFormatString("Command list close Error : 0x%x\n", result);
		return false;
	}

	// Note: コマンドリストを実行
	ID3D12CommandList* commandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, commandLists);

	// Note: Flip
	_swapchain->Present(1, 0);

	commandQueue->Signal(fence, ++fenceValue);
	if (fence->GetCompletedValue() != fenceValue) {
		auto event = CreateEvent(nullptr, false, false, nullptr);
		fence->SetEventOnCompletion(fenceValue, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	return true;
}

#ifdef _DEBUG
int main()
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#endif // _DEBUG
{
	DebugOutputFormatString("Show window test.\n");
	WNDCLASSEX w = {};

	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = WindowProcedure;
	w.lpszClassName = _T("DX12Sample");
	w.hInstance = GetModuleHandle(nullptr);

	RegisterClassEx(&w);

	RECT windowRect = { 0, 0, window_width, window_height };

	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(
		w.lpszClassName,
		_T("DirectX12テスト"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		w.hInstance,
		nullptr
	);

#ifdef _DEBUG
	EnableDebugLayer();
#endif // _DEBUG

	D3D_FEATURE_LEVEL detectedFeatureLevel;
	InitDirect3DDevice(detectedFeatureLevel);

#ifdef _DEBUG
	CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif
	IDXGIAdapter* tmpAdapter = nullptr;
	SetDXGIAdapter(&tmpAdapter);

	ID3D12CommandAllocator* _commandAllocator = nullptr;
	ID3D12GraphicsCommandList* _commandList = nullptr;
	if (!CreateD3D12CommandListAndAllocator(_commandAllocator, _commandList)) {
		return -1;
	}

	ID3D12CommandQueue* _commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC _commandQueueDesc = {};
	if (!CreateD3D12CommandQueue(_commandQueue, _commandQueueDesc)) {
		return -2;
	}

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc1 = {};
	if (!CreateD3D12SwapChain(swapchainDesc1, _commandQueue, hwnd)) {
		return -3;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	ID3D12DescriptorHeap* rtvHeap = nullptr;
	if (!CreateD3D12DescriptorHeap(rtvHeap, heapDesc)) {
		return -4;
	}

	std::vector<ID3D12Resource*> backBuffers;
	if (!AssociateDescriptorAndBackBufferOnSwapChain(rtvHeap, backBuffers)) {
		return -5;
	}

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceValue = 0;
	if (!SetupFence(_fenceValue, &_fence)) {
		return -6;
	}

	ShowWindow(hwnd, SW_SHOW);

	Vertex vertices[] = {
		{{-0.4f, -0.7f, 0.0f}, {0.0f, 1.0f}},
		{{-0.4f,  0.7f, 0.0f}, {0.0f, 0.0f}},
		{{ 0.4f, -0.7f, 0.0f}, {1.0f, 1.0f}},
		{{ 0.4f,  0.7f, 0.0f}, {1.0f, 0.0f}},
	};
	ID3D12Resource* vertexBuffer = nullptr;

	unsigned short indices[] = {
		0, 1, 2,
		2, 1, 3,
	};
	ID3D12Resource* indexBuffer = nullptr;

	if (!SetupVertexBuffer(vertices, &vertexBuffer, indices, &indexBuffer)) {
		return -7;
	}

	Vertex* verticesMap = nullptr;
	auto result = vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&verticesMap));
	if (FAILED(result)) {
		DebugOutputFormatString("Vertex buffer map Error : 0x%x\n", result);
		return -12;
	}
	std::copy(std::begin(vertices), std::end(vertices), verticesMap);
	vertexBuffer->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.SizeInBytes = sizeof(vertices);
	vertexBufferView.StrideInBytes = sizeof(vertices[0]);

	// 作ったバッファーにインデックスデータをバッファーにコピー
	unsigned short* indicesMap = nullptr;
	result = indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indicesMap));
	if (FAILED(result)) {
		DebugOutputFormatString("Index buffer map Error : 0x%x\n", result);
		return -13;
	}
	std::copy(std::begin(indices), std::end(indices), indicesMap);
	indexBuffer->Unmap(0, nullptr);

	// インデックスバッファービューを作成
	D3D12_INDEX_BUFFER_VIEW indexBufferView = {};

	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	indexBufferView.SizeInBytes = sizeof(indices);

	ID3D10Blob* vsBlob = nullptr;
	ID3D10Blob* psBlob = nullptr;
	ID3D10Blob* errorBlob = nullptr;

	result = D3DCompileFromFile(
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob,
		&errorBlob
	);
	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			DebugOutputFormatString("Vertex Shader File not found.\n");
			return -10;
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
			"D3DCompileFromFile Vertex Shader Error : 0x%x\n",
			errorMessage.c_str()
		);
		return -8;
	}

	result = D3DCompileFromFile(
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob,
		nullptr
	);
	if (FAILED(result)) {
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
			DebugOutputFormatString("Pixel Shader File not found.\n");
			return -9;
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
			"D3DCompileFromFile Pixel Shader Error : 0x%x\n",
			errorMessage.c_str()
		);
		return -10;
	}

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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipeline = {};

	graphicsPipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	graphicsPipeline.VS.BytecodeLength = vsBlob->GetBufferSize();
	graphicsPipeline.PS.pShaderBytecode = psBlob->GetBufferPointer();
	graphicsPipeline.PS.BytecodeLength = psBlob->GetBufferSize();

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

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
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


	D3D12_DESCRIPTOR_RANGE descriptorRange = {};
	// 種別はテクスチャ
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	// テクスチャ１つ
	descriptorRange.NumDescriptors = 1;
	// 0版スロットから
	descriptorRange.BaseShaderRegister = 0;
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameter = {};
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	// ピクセルシェーダーから見えるようにする
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	// ディスクリプタレンジのアドレス
	rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;
	// ディスクリプタレンジ数
	rootParameter.DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};

	// // 線形補間
	// samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	// 補間しない(ニアレストネイバー法: 最近傍補間)
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
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

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	rootSignatureDesc.pParameters = &rootParameter;
	rootSignatureDesc.NumParameters = 1;
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSignatureBlob = nullptr;
	result = D3D12SerializeRootSignature(
		// ルートシグネチャ設定
		&rootSignatureDesc,
		// ルートシグネチャバージョン
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		// シェーダーを作ったときと同じ
		&rootSignatureBlob,
		// エラー処理も同じ
		&errorBlob
	);
	if (FAILED(result)) {
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
		return -17;
	}

	ID3D12RootSignature* rootSignature = nullptr;
	result = _dev->CreateRootSignature(
		// nodeMask。0でよい
		0,
		// シェーダーの時と同様
		rootSignatureBlob->GetBufferPointer(),
		// シェーダーの時と同様
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateRootSignature Error : 0x%x\n", result);
		return -18;
	}
	// 不要になったので解放
	rootSignatureBlob->Release();

	graphicsPipeline.pRootSignature = rootSignature;


	ID3D12PipelineState* pipelineState = nullptr;
	result = _dev->CreateGraphicsPipelineState(
		&graphicsPipeline,
		IID_PPV_ARGS(&pipelineState)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateGraphicsPipelineState Error : 0x%x\n", result);
		return -11;
	}

	D3D12_VIEWPORT viewport = {};

	// 出力先の幅(ピクセル数)
	viewport.Width = window_width;
	// 出力先の高さ(ピクセル数)
	viewport.Height = window_height;
	// 出力先の左上X座標
	viewport.TopLeftX = 0.0f;
	// 出力先の左上Y座標
	viewport.TopLeftY = 0.0f;
	// 深度最大値
	viewport.MaxDepth = 1.0f;
	// 深度最小値
	viewport.MinDepth = 0.0f;

	D3D12_RECT scissorRect = {};

	// 切り抜き上座標
	scissorRect.top = 0;
	// 切り抜き左座標
	scissorRect.left = 0;
	// 切り抜き右座標
	scissorRect.right = scissorRect.left + window_width;
	// 切り抜き下座標
	scissorRect.bottom = scissorRect.top + window_height;

	std::vector<TexRGBA> textureData(256 * 256);
	for (auto& rgba: textureData)
	{
		rgba.R = rand() % 256;
		rgba.G = rand() % 256;
		rgba.B = rand() % 256;
		// αは表示するため1.0
		rgba.A = 255;
	}

	// WriteToSubresource で転送するためのヒープ設定
	D3D12_HEAP_PROPERTIES heapProperties = {};

	// 特殊な設定なので DEFAULT でも UPLOAD でもない
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM;

	// ライトバック
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;

	// 転送は L0、つまり CPU 側 から直接行う
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	// 単一アダプターのため0
	heapProperties.CreationNodeMask = 0;
	heapProperties.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resourceDescription = {};

	// RGBA フォーマット
	resourceDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	// 幅
	resourceDescription.Width = 256;
	// 高さ
	resourceDescription.Height = 256;
	// 2D で配列でもないので1
	resourceDescription.DepthOrArraySize = 1;
	resourceDescription.SampleDesc = {
		// 通常のテクスチャなのでアンチエイリアシングは使わない
		1,
		// クオリティは最低
		0
	};
	// ミップマップしないのでミップ数は1
	resourceDescription.MipLevels = 1;
	// 2D テクスチャ用
	resourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	// レイアウトは決定しない
	resourceDescription.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	// 特にフラグなし
	resourceDescription.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* textureBuffer = nullptr;
	result = _dev->CreateCommittedResource(
		&heapProperties,
		// 特に指定なし
		D3D12_HEAP_FLAG_NONE,
		&resourceDescription,
		// テクスチャ用指定
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&textureBuffer)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommittedResource Error (for texture): 0x%x\n", result);
		return -14;
	}

	result = textureBuffer->WriteToSubresource(
		0,
		// 全領域に書き込む
		nullptr,
		// 元データアドレス
		textureData.data(),
		// 1ライン分のバイト数
		sizeof(TexRGBA) * 256,
		// 全体のバイト数
		sizeof(TexRGBA) * textureData.size()
	);
	if (FAILED(result)) {
		DebugOutputFormatString("WriteToSubresource Error : 0x%x\n", result);
		return -15;
	}

	ID3D12DescriptorHeap* textureDescriptionHeap = nullptr;
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
	result = _dev->CreateDescriptorHeap(
		&textureHeapDesc,
		IID_PPV_ARGS(&textureDescriptionHeap)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateDescriptorHeap Error (for texture): 0x%x\n", result);
		return -16;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	// RGBA (0.0f～1.0f に正規化)
	// 本では DXGI_FORMAT_R8G8B8A8_UNORM となっているが、Copilot に従い、resourceDescription.Format を流用
	srvDesc.Format = resourceDescription.Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	// 2D テクスチャ
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	// ミップマップは使用しないので1
	srvDesc.Texture2D.MipLevels = 1;

	_dev->CreateShaderResourceView(
		textureBuffer,
		&srvDesc,
		textureDescriptionHeap->GetCPUDescriptorHandleForHeapStart()
	);

	MSG msg = {};

	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT) {
			break;
		}

		// Note: DirectX の処理
		ExecuteDirectXProcedure(
			rtvHeap,
			_commandList,
			_commandQueue,
			_commandAllocator,
			_fence,
			_fenceValue,
			backBuffers,
			pipelineState,
			rootSignature,
			viewport,
			scissorRect,
			vertexBufferView,
			indexBufferView,
			textureDescriptionHeap
		);
	}

	UnregisterClass(w.lpszClassName, w.hInstance);
	
	return 0;
}
