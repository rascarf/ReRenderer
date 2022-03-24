#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <d3dx12/d3dx12.h>
#include <d3dcompiler.h>
#include <memory>

#include "Descriptor.h"


using Microsoft::WRL::ComPtr;

class UploadBuffer
{
public:

    ComPtr<ID3D12Resource> Buffer;
    UINT Capacity;
    UINT Cursor;
    D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
    uint8_t* CpuAddress;

    static UploadBuffer CreateUploadBuffer(ComPtr<ID3D12Device>m_Device,UINT Capacity);
   
};

class UploadBufferRegion
{
public:
    void* CpuAddress;
    D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
    UINT Size;

    static UploadBufferRegion AllocFromUploadBuffer(
        UploadBuffer& Buffer,
        UINT Size,
        int Align
    );

};

class ConstantBufferView
{
public:

    UploadBufferRegion Data;
    Descriptor Cbv;


    template<typename T>T* as() const
    {
        return reinterpret_cast<T*>(Data.CpuAddress);
    }

    static ConstantBufferView CreateConstantBufferView(
        const void* Data,
        UINT size,
        ComPtr<ID3D12Device> m_Device,
        DescriptorHeap& m_DescHeapCBV_SRV_UAV, 
        UploadBuffer& m_constantBuffer
    );

    template<typename T>
    static ConstantBufferView CreateConstantBufferView
    (
        ComPtr<ID3D12Device> m_Device,
        DescriptorHeap& m_DescHeapCBV_SRV_UAV,
        UploadBuffer& m_constantBuffer,
        const T* Data = nullptr)
    {
        return CreateConstantBufferView(Data, sizeof(T),m_Device,m_DescHeapCBV_SRV_UAV,m_constantBuffer);
    }

};

