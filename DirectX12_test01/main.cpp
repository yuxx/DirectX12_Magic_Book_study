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

void SetDXGIAdapter(IDXGIAdapter* tmpAdapter)
{
	std::vector<IDXGIAdapter*> adapters;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (const auto adapter: adapters)
	{
		DXGI_ADAPTER_DESC adapterDesc = {};
		adapter->GetDesc(&adapterDesc);

		std::wstring desc = adapterDesc.Description;

		if (desc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adapter;
			printf("NVIDIA Video card found!\n");
			break;
		}
	}
}

bool CreateD3D12CommandListAndAllocator(
	ID3D12CommandAllocator* commandAllocator,
	ID3D12GraphicsCommandList* commandList
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

	InitDirect3DDevice();

	auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
	IDXGIAdapter* tmpAdapter = nullptr;
	SetDXGIAdapter(tmpAdapter);

	ID3D12CommandAllocator* _commandAllocator = nullptr;
	ID3D12GraphicsCommandList* _commandList = nullptr;
	if (!CreateD3D12CommandListAndAllocator(_commandAllocator, _commandList))
	{
		return -1;
	}

	ID3D12CommandQueue* _commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC _commandQueueDesc = {};
	if (!CreateD3D12CommandQueue(&_commandQueue, _commandQueueDesc)) {
		return -2;
	}

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	if (!CreateD3D12SwapChain(swapchainDesc, _commandQueue, hwnd))
	{
		return -3;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	ID3D12DescriptorHeap* _rtvHeap = nullptr;
	if (!CreateD3D12DescriptorHeap(&_rtvHeap, heapDesc))
	{
		return -4;
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
	}

	UnregisterClass(w.lpszClassName, w.hInstance);
	
	return 0;
}
