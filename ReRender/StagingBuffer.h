#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <memory>
#include <vector>
#include <wrl/client.h>


class StagingBuffer
{
public:
    Microsoft::WRL::ComPtr<ID3D12Resource> Buffer;

    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> Layouts;

    UINT FirstSubResource;

    UINT NumSubResource;

    static StagingBuffer CreateStagingBuffer(
        Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
        const Microsoft::WRL::ComPtr<ID3D12Resource>& Resource,
        UINT FirstSubresource,
        UINT NumSubResources,
        const D3D12_SUBRESOURCE_DATA* Data
    );
};


