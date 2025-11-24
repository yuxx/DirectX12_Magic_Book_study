#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <DirectXMath.h>

#include "DirectXManager.h"

#ifdef _DEBUG
#include <iostream>
#endif // _DEBUG

using namespace std;
using namespace yuxx::DirectX12;

constexpr unsigned int g_window_width = 1280;
constexpr unsigned int g_window_height = 720;

#ifdef _DEBUG
int main() {
	HINSTANCE hInstance = GetModuleHandle(nullptr);
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#endif // _DEBUG

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) {
		return -1;
	}

	{
		DirectXManager dxManager;
		if (!dxManager.Initialize(hInstance, g_window_width, g_window_height)) {
			return -2;
		}

		MSG msg = {};

		while (true) {
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			if (msg.message == WM_QUIT) {
				break;
			}

			dxManager.Render();
		}
	}

	CoUninitialize();
	
	return 0;
}
