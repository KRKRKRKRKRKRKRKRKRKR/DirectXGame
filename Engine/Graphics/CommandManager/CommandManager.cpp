#include "CommandManager.h"
#include "../../Utils/Logger.h"
#include <cassert>

void CommandManager::Initialize(ID3D12Device* device) {
	CreateCommandQueue(device);
	CreateCommandAllocator(device);
	CreateCommandList(device);
	CreateFence(device);
	fenceValue_ = 0;
}

void CommandManager::CreateCommandQueue(ID3D12Device* device) {
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	HRESULT hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandQueue\n");
	}
	assert(SUCCEEDED(hr));
}

void CommandManager::CreateCommandAllocator(ID3D12Device* device) {
	HRESULT hr = device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandAllocator\n");
	}
	assert(SUCCEEDED(hr));
}

void CommandManager::CreateCommandList(ID3D12Device* device) {
	HRESULT hr = device->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr,
		IID_PPV_ARGS(&commandList_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommandList\n");
	}
	assert(SUCCEEDED(hr));
}

void CommandManager::CreateFence(ID3D12Device* device) {
	HRESULT hr = device->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	if (FAILED(hr)) {
		Logger::Log("Failed CreateFence\n");
	}
	assert(SUCCEEDED(hr));

	fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	assert(fenceEvent_ != NULL);
}

void CommandManager::ExecuteCommandList() {
	ID3D12CommandList* commandLists[] = { commandList_.Get() };
	commandQueue_->ExecuteCommandLists(_countof(commandLists), commandLists);
}

void CommandManager::Signal() {
	fenceValue_++;
	commandQueue_->Signal(fence_.Get(), fenceValue_);
}

void CommandManager::WaitForFence() {
	if (fence_->GetCompletedValue() < fenceValue_) {
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
}

void CommandManager::ResetCommandList() {
	commandAllocator_->Reset();
	commandList_->Reset(commandAllocator_.Get(), nullptr);
}

void CommandManager::Finalize() {
	CloseHandle(fenceEvent_);
	commandList_.Reset();
	commandAllocator_.Reset();
	commandQueue_.Reset();
	fence_.Reset();
}
