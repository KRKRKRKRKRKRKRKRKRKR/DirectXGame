#include "ResourceFactory.h"
#include "../../Utils/Logger.h"
#include <cassert>

ComPtr<ID3D12Resource> ResourceFactory::CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	ComPtr<ID3D12Resource> resource = nullptr;
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC vertexBufferResourceDesc{};
	vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertexBufferResourceDesc.Width = sizeInBytes;
	vertexBufferResourceDesc.Height = 1;
	vertexBufferResourceDesc.DepthOrArraySize = 1;
	vertexBufferResourceDesc.MipLevels = 1;
	vertexBufferResourceDesc.SampleDesc.Count = 1;
	vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device->CreateCommittedResource(
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&vertexBufferResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&resource)
	);

	if (FAILED(hr)) {
		Logger::Log("Failed CreateCommittedResource\n");
	}
	assert(SUCCEEDED(hr));

	return resource;
}