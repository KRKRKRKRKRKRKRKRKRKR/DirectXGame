#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl.h>
#include <assert.h>
#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
using Microsoft::WRL::ComPtr;


class DeviceManager {
public:
	DeviceManager() = default;
	~DeviceManager() = default;
	void Initialize(HWND hwnd, int32_t width, int32_t height);

private:
	//DirectXAPIへのアクセスを管理する
	void CreateFactory();

	//最適なGPUアダプタの選択
	void SelectAdapter();

	//デバイスの作成
	void CreateDevice();

	//コマンドキューの作成
	void CreateCommandQueue();

	//スワップチェーンの作成
	void CreateSwapChain(HWND hwnd);

	//スワップチェーンのリソースの取得
	void GetSwapChainResources();

	//同期のためのフェンスの作成
	void CreateFence();
	
	//Descriptorサイズの取得
	void SetDescriptorSizes();
	//DXGIFactoryとAdapter、Deviceの管理
	ComPtr<IDXGIFactory7> dxgiFactory_ = nullptr;
	ComPtr<IDXGIAdapter4> useAdapter_ = nullptr;
	ComPtr<ID3D12Device> device_ = nullptr;

	//コマンドキューとコマンドリストの管理
	ComPtr<ID3D12CommandQueue> commandQueue_ = nullptr;
	ComPtr<ID3D12GraphicsCommandList> commandList_ = nullptr;
	ComPtr<ID3D12CommandAllocator> commandAllocator_ = nullptr;

	//スワップチェーンの管理
	ComPtr<IDXGISwapChain4> swapChain_ = nullptr;
	ComPtr<ID3D12Resource> swapChainResources_[2] = { nullptr, nullptr };
	int32_t windowWidth_ = 0;
	int32_t windowHeight_ = 0;

	//フェンスの管理
	ComPtr<ID3D12Fence> fence_ = nullptr;
	UINT64 fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	//Descriptorサイズの管理
	uint32_t descriptorSizeSRV_;
	uint32_t descriptorSizeRTV_;
	uint32_t descriptorSizeDSV_;

};
