#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#ifdef _DEBUG
#include <iostream>
#endif // _DEBUG

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace std;

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

void InitDirect3DDevice()
{
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};
	D3D_FEATURE_LEVEL detectedFeatureLevel;
	for (auto fl : featureLevels) {
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
			printf("NVIDIA Video card found!\n");
			break;
		}
	}
}

bool CreateD3D12CommandListAndAllocator(
	ID3D12CommandAllocator** commandAllocator,
	ID3D12GraphicsCommandList** commandList
) {
	auto result = _dev->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(commandAllocator)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommandAllocator Error : 0x%x\n", result);
		return false;
	}
	result = _dev->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		*commandAllocator,
		nullptr,
		IID_PPV_ARGS(commandList)
	);
	if (FAILED(result)) {
		DebugOutputFormatString("CreateCommandList Error : 0x%x\n", result);
		return false;
	}

	return true;
}

bool CreateD3D12CommandQueue(
	ID3D12CommandQueue** commandQueue,
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
		IID_PPV_ARGS(commandQueue)
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
	ID3D12DescriptorHeap** rtvHeap,
	D3D12_DESCRIPTOR_HEAP_DESC& heapDesc
) {
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	auto result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(rtvHeap));
	if (FAILED(result)) {
		DebugOutputFormatString("CreateDescriptorHeap Error : 0x%x\n", result);
		return false;
	}

	return true;
}

bool AccociateDescriptorAndBackBufferOnSwapChain(ID3D12DescriptorHeap* rtvHeap)
{
	DXGI_SWAP_CHAIN_DESC swapchainDesc = {};
	auto result = _swapchain->GetDesc(&swapchainDesc);
	if (FAILED(result)) {
		DebugOutputFormatString("GetDesc Error : 0x%x\n", result);
		return false;
	}
	std::vector<ID3D12Resource*> _backBuffers(swapchainDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (size_t i = 0; i < swapchainDesc.BufferCount; ++i) {
		result = _swapchain->GetBuffer(i, IID_PPV_ARGS(&_backBuffers[i]));
		if (FAILED(result)) {
			DebugOutputFormatString("GetBuffer Error : 0x%x\n", result);
			return false;
		}
		_dev->CreateRenderTargetView(_backBuffers[i], nullptr, rtvHandle);
		rtvHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	return true;
}

bool ExecuteDirectXProcedure(
	ID3D12DescriptorHeap* rtvHeap,
	UINT backBufferIndex,
	ID3D12GraphicsCommandList* _commandList,
	ID3D12CommandQueue* _commandQueue,
	ID3D12CommandAllocator* _commandAllocator,
	ID3D12Fence* _fence,
	UINT64* _fenceValue
) {
	// Note: レンダーターゲットの設定
	auto rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += backBufferIndex * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_commandList->OMSetRenderTargets(1, &rtvHandle, true, nullptr);

	// Note: 画面をクリア
	float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };
	_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	_commandList->Close();

	ID3D12CommandList* commandLists[] = { _commandList };
	_commandQueue->ExecuteCommandLists(1, commandLists);

	_commandQueue->Signal(_fence, ++*_fenceValue);
	if (_fence->GetCompletedValue() != *_fenceValue) {
		auto event = CreateEvent(nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion(*_fenceValue, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	// Note: クリア
	auto result = _commandAllocator->Reset();
	if (FAILED(result)) {
		DebugOutputFormatString("Command allocator reset Error : 0x%x\n", result);
		return false;
	}
	result = _commandList->Reset(_commandAllocator, nullptr);
	if (FAILED(result)) {
		DebugOutputFormatString("Command list reset Error : 0x%x\n", result);
		return false;
	}

	_swapchain->Present(1, 0);

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
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
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

	InitDirect3DDevice();

#ifdef _DEBUG
	CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif
	IDXGIAdapter* tmpAdapter = nullptr;
	SetDXGIAdapter(&tmpAdapter);

	ID3D12CommandAllocator* _commandAllocator = nullptr;
	ID3D12GraphicsCommandList* _commandList = nullptr;
	if (!CreateD3D12CommandListAndAllocator(&_commandAllocator, &_commandList))
	{
		return -1;
	}

	ID3D12CommandQueue* _commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC _commandQueueDesc = {};
	if (!CreateD3D12CommandQueue(&_commandQueue, _commandQueueDesc)) {
		return -2;
	}

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc1 = {};
	if (!CreateD3D12SwapChain(swapchainDesc1, _commandQueue, hwnd))
	{
		return -3;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	ID3D12DescriptorHeap* rtvHeap = nullptr;
	if (!CreateD3D12DescriptorHeap(&rtvHeap, heapDesc))
	{
		return -4;
	}

	if (!AccociateDescriptorAndBackBufferOnSwapChain(rtvHeap))
	{
		return -5;
	}

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceValue = 0;
	auto result = _dev->CreateFence(_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
	if (FAILED(result)) {
		DebugOutputFormatString("CreateFence Error : 0x%x\n", result);
		return -6;
	}

	ShowWindow(hwnd, SW_SHOW);

	MSG msg = {};

	while (true) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT) {
			break;
		}

		auto backBufferIndex = _swapchain->GetCurrentBackBufferIndex();
		ExecuteDirectXProcedure(
			rtvHeap,
			backBufferIndex,
			_commandList,
			_commandQueue,
			_commandAllocator,
			_fence,
			&_fenceValue
		);
	}

	UnregisterClass(w.lpszClassName, w.hInstance);
	
	return 0;
}
