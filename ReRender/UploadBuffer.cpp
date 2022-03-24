#include "UploadBuffer.h"
#include "Utils.h"
#include <stdexcept>

UploadBuffer UploadBuffer::CreateUploadBuffer(ComPtr<ID3D12Device>m_Device,UINT Capacity)
{
    UploadBuffer buffer;
    buffer.Cursor = 0;
    buffer.Capacity = Capacity;

    auto HeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(Capacity);

    if (FAILED(m_Device->CreateCommittedResource(
        &HeapProperty,
        D3D12_HEAP_FLAG_NONE,
        &ResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer.Buffer))))
    {
        throw std::runtime_error("Failed to create GPU upload buffer");
    }

    auto Range = CD3DX12_RANGE{ 0, 0 };
    if (FAILED(buffer.Buffer->Map(0, &Range, reinterpret_cast<void**>(&buffer.CpuAddress))))
    {
        throw std::runtime_error("Failed to map GPU upload buffer to host address space");
    }
    buffer.GpuAddress = buffer.Buffer->GetGPUVirtualAddress();
    return buffer;
}

UploadBufferRegion UploadBufferRegion::AllocFromUploadBuffer(
    UploadBuffer& Buffer,
    UINT Size,
    int Align)
{
    const UINT AlignedSize = Utils::roundToPowerOfTwo(Size, Align);
    if (Buffer.Cursor + AlignedSize > Buffer.Capacity)
    {
        throw std::overflow_error("Out of upload buffer capacity while allocating memory");
    }

    UploadBufferRegion Region;
    Region.CpuAddress = reinterpret_cast<void*>(Buffer.CpuAddress + Buffer.Cursor);
    Region.GpuAddress = Buffer.GpuAddress + Buffer.Cursor;
    Region.Size = AlignedSize;
    Buffer.Cursor += AlignedSize;

    return Region;
}

ConstantBufferView ConstantBufferView::CreateConstantBufferView(const void* Data, UINT size, ComPtr<ID3D12Device> m_Device, DescriptorHeap& m_DescHeapCBV_SRV_UAV,UploadBuffer& m_constantBuffer)
{
    ConstantBufferView View;
    View.Data = UploadBufferRegion::AllocFromUploadBuffer(m_constantBuffer, size, 256);
    View.Cbv = m_DescHeapCBV_SRV_UAV.Alloc();
    if (Data)
    {
        std::memcpy(View.Data.CpuAddress, Data, size);
    }

    D3D12_CONSTANT_BUFFER_VIEW_DESC Desc = {};
    Desc.BufferLocation = View.Data.GpuAddress;
    Desc.SizeInBytes = View.Data.Size;
    m_Device->CreateConstantBufferView(&Desc, View.Cbv.CpuHandle);

    return View;
}
