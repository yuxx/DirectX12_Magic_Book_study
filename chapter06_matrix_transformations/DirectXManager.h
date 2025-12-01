#pragma once
#include <d3d12.h>
#include <DirectXTex.h>
#include <dxgi1_6.h>
#include <wrl.h>

using Microsoft::WRL::ComPtr;

namespace yuxx {
namespace DirectX12 {
class DirectXManager
{
public:
	struct Vertex {
		DirectX::XMFLOAT3 position; // xyzç¿ïW
		DirectX::XMFLOAT2 uv;       // uvç¿ïW
	};

	struct TexRGBA
	{
		unsigned char R, G, B, A;
	};

	DirectXManager();
	~DirectXManager();
	bool Initialize(HINSTANCE hInstance, int width, int height);
	bool Render();

private:
	static constexpr Vertex kVertices[] = {
		{{-0.4f, -0.7f, 0.0f}, {0.0f, 1.0f}},
		{{-0.4f,  0.7f, 0.0f}, {0.0f, 0.0f}},
		{{ 0.4f, -0.7f, 0.0f}, {1.0f, 1.0f}},
		{{ 0.4f,  0.7f, 0.0f}, {1.0f, 0.0f}},
	};
	static constexpr unsigned short kIndices[] = {
		0, 1, 2,
		2, 1, 3,
	};

	WNDCLASSEX m_windowClass = {};
	HWND m_hwnd = nullptr;
	RECT m_windowRect = { 0, 0, 320, 240 };

	ComPtr<ID3D12Device> m_device;
	ComPtr<IDXGIFactory6> m_dxgiFactory;
	ComPtr<IDXGISwapChain4> m_swapChain;
	ComPtr<IDXGIAdapter> m_adapter;
	D3D_FEATURE_LEVEL m_feature_level = D3D_FEATURE_LEVEL_11_0;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	std::vector<ComPtr<ID3D12Resource>> m_backBuffers;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue = 0;

	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView{};
	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView{};

	ComPtr<ID3D10Blob> m_vsBlob;
	ComPtr<ID3D10Blob> m_psBlob;

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	D3D12_VIEWPORT m_viewport = {};
	D3D12_RECT m_scissorRect = {};

	DirectX::TexMetadata m_metadata{};

	ComPtr<ID3D12Resource> m_textureBuffer;
	ComPtr<ID3D12DescriptorHeap> m_textureDescriptionHeap;

	bool MakeWindow(HINSTANCE hInstance, int width, int height);
	bool SelectAdapter();
	bool InitDirect3DDevice();
	bool InitCommandAllocatorAndCommandQueue();
	bool InitSwapChain();
	bool InitRTV();
	bool InitFence();

	bool SetupVertexBuffer();
	bool SetupShaders();
	bool SetupGraphicsPipeline();
	void SetupViewportAndScissor(unsigned int windowWidth, unsigned int windowHeight);
	static void SetupUploadHeap(
		D3D12_HEAP_PROPERTIES& uploadHeapProperties,
		D3D12_RESOURCE_DESC& uploadResourceDescription,
		const DirectX::Image* image
	);
	void SetupTextureHeap(
		D3D12_HEAP_PROPERTIES& textureHeapProperties,
		D3D12_RESOURCE_DESC& resourceDescription
	);
	static size_t AlignedSize(size_t size, size_t alignment);
	void SetupTextureBufferLocation(
		D3D12_TEXTURE_COPY_LOCATION& srcLocation,
		D3D12_TEXTURE_COPY_LOCATION& dstLocation,
		ID3D12Resource* uploadBuffer,
		const DirectX::TexMetadata& metadata,
		const DirectX::Image* image
	) const;
	void SetupTextureResourceBarrier(D3D12_RESOURCE_BARRIER& textureResourceBarrier) const;
	bool LoadTexture();
	bool MakeShaderResourceView();

	static bool EnableDebugLayer();
};
}
}
