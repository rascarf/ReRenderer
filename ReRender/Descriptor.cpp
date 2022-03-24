#include "Descriptor.h"

#include <stdexcept>

DescriptorHeap DescriptorHeap::CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device> m_Device, const D3D12_DESCRIPTOR_HEAP_DESC& Desc)
{
    DescriptorHeap Heap;
    if (FAILED(m_Device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&Heap.Heap))))
    {
        throw std::runtime_error("Falied to Create Descriptor Heap");
    }

    Heap.NumDescriptorsAllocated = 0;
    Heap.NumDescriptorsInHeap = Desc.NumDescriptors;
    Heap.DescriptorSize = m_Device->GetDescriptorHandleIncrementSize(Desc.Type);

    return Heap;
}
