#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

class CommandManager {
public:
	void Initialize(ID3D12Device* device);

	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }

	void ExecuteCommandList();
	void Signal();
	void WaitForFence();
	void ResetCommandList();

	void Finalize();

private:
	ComPtr<ID3D12CommandQueue> commandQueue_;
	ComPtr<ID3D12CommandAllocator> commandAllocator_;
	ComPtr<ID3D12GraphicsCommandList> commandList_;
	ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_;
	HANDLE fenceEvent_;

	void CreateCommandQueue(ID3D12Device* device);
	void CreateCommandAllocator(ID3D12Device* device);
	void CreateCommandList(ID3D12Device* device);
	void CreateFence(ID3D12Device* device);
};
