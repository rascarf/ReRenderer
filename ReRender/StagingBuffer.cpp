#include "StagingBuffer.h"

#include <cassert>
#include <stdexcept>
#include <d3dx12/d3dx12.h>

StagingBuffer StagingBuffer::CreateStagingBuffer(Microsoft::WRL::ComPtr<ID3D12Device> m_Device, const Microsoft::WRL::ComPtr<ID3D12Resource>& Resource, UINT FirstSubresource, UINT NumSubResources, const D3D12_SUBRESOURCE_DATA* Data)
{
    StagingBuffer stagingBuffer;
    stagingBuffer.FirstSubResource = FirstSubresource;
    stagingBuffer.NumSubResource = NumSubResources;
    stagingBuffer.Layouts.resize(NumSubResources);

    const D3D12_RESOURCE_DESC ResourceDesc = Resource->GetDesc();

    UINT64 NumBytesTotal;
    std::vector<UINT> numRows{ NumSubResources };
    std::vector<UINT64> rowBytes{ NumSubResources };
    m_Device->GetCopyableFootprints(
        &ResourceDesc,
        FirstSubresource,
        NumSubResources,
        0,
        stagingBuffer.Layouts.data(),
        numRows.data(),
        rowBytes.data(),
        &NumBytesTotal);


    auto UploadType = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(NumBytesTotal);
    if (FAILED(m_Device->CreateCommittedResource(
        &UploadType,
        D3D12_HEAP_FLAG_NONE,
        &BufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&stagingBuffer.Buffer)
    )))
    {
        throw std::runtime_error("Failed to create GPU staging buffer");
    }

    if (Data)
    {
        assert(ResourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D);

        auto Range = CD3DX12_RANGE{ 0,0 };
        void* BufferMemory;
        if (FAILED(stagingBuffer.Buffer->Map(0, &Range, &BufferMemory)))
        {
            throw std::runtime_error("Failed to map GPU staging buffer to host address space");
        }

        for (UINT subresource = 0; subresource < NumSubResources; ++subresource)
        {
            uint8_t* subresourceMemory = reinterpret_cast<uint8_t*>(BufferMemory) + stagingBuffer.Layouts[subresource].Offset;

            //线性的话就一把梭
            if (ResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
            {
                std::memcpy(subresourceMemory, Data->pData, NumBytesTotal);
            }
            else
            {
                //Texture就需要逐行复制
                for (UINT row = 0; row < numRows[subresource]; ++row)
                {
                    const uint8_t* srcRowPtr = reinterpret_cast<const uint8_t*>(Data[subresource].pData) + row * Data[subresource].RowPitch;

                    uint8_t* destRowPtr = subresourceMemory + row * stagingBuffer.Layouts[subresource].Footprint.RowPitch;

                    std::memcpy(destRowPtr, srcRowPtr, rowBytes[subresource]);
                }
            }
        }

        stagingBuffer.Buffer->Unmap(0, nullptr);
    }

    return stagingBuffer;
}
