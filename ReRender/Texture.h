#pragma once
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <functional>
#include <memory>
#include <wrl/client.h>
#include "Image.h"

#include "Descriptor.h"

struct MipMapGeneration
{
    Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> LinearTexturePipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> gammaTexturePipelineState;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>ArrayTexturePipelineState;
};


class Texture
{
public:
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    Descriptor Srv;
    Descriptor Uav;
    UINT Width, Height;
    UINT Levels;

    //Texture
    static Texture CreateTexture(
        Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
        DescriptorHeap& m_DescHeapCBV_SRV_UAV,
        UINT Width,
        UINT Height,
        UINT Depth,
        DXGI_FORMAT Format,
        UINT Levels = 0,
        D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
    );

    static Texture CreateTexture(
        std::function<void()>CallBack ,
        Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList,
        MipMapGeneration& m_mipmapGeneration,
        DescriptorHeap& m_DescHeapCBV_SRV_UAV,
        D3D_ROOT_SIGNATURE_VERSION& m_RootSignatureVersion,
        const std::shared_ptr<class Image>& Image, DXGI_FORMAT Format,
        UINT Levels = 0
    );

    static void GenerateMipmaps(
        std::function<void()> CallBack ,
        Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList,
        MipMapGeneration& m_mipmapGeneration,
        DescriptorHeap& m_DescHeapCBV_SRV_UAV,
        D3D_ROOT_SIGNATURE_VERSION& m_RootSignatureVersion,
        const Texture& texture);

    static void CreateTextureSRV(
        Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
        DescriptorHeap& m_DescHeapCBV_SRV_UAV,
        Texture& texture,
        D3D12_SRV_DIMENSION Dimension,
        UINT MostDetailedMip = 0,
        UINT MipLevels = 0,
        bool bIsOnlyDepth = false
    );

    static void CreateTextureUAV(
        Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
        DescriptorHeap& m_DescHeapCBV_SRV_UAV,
        Texture& texture,
        UINT mipSlice
    );

};

