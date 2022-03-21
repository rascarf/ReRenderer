#pragma once
#include <cassert>
#include <d3d12.h>
#include <wrl/client.h>

class Descriptor
{
public:

    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle;

    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle;
    
};

struct DescriptorHeap
{
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Heap;
    UINT DescriptorSize;
    UINT NumDescriptorsInHeap;
    UINT NumDescriptorsAllocated;

    Descriptor Alloc()
    {
        return (*this)[NumDescriptorsAllocated++];
    }

    Descriptor operator[](UINT Index) const
    {
        assert(Index < NumDescriptorsInHeap);
        return {
            D3D12_CPU_DESCRIPTOR_HANDLE{Heap->GetCPUDescriptorHandleForHeapStart().ptr + Index * DescriptorSize},
            D3D12_GPU_DESCRIPTOR_HANDLE{Heap->GetGPUDescriptorHandleForHeapStart().ptr + Index * DescriptorSize }
        };
    }
};

struct DescriptorHeapMark
{
    DescriptorHeapMark(DescriptorHeap& InHeap)
        :Heap(InHeap)
        , Mark(Heap.NumDescriptorsAllocated)
    {}

    ~DescriptorHeapMark()
    {
        Heap.NumDescriptorsAllocated = Mark;
    }

    DescriptorHeap& Heap;
    const UINT Mark;
};

