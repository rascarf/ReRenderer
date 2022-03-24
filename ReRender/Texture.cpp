#include "Texture.h"

#include <stdexcept>
#include <d3dx12/d3dx12.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/include/glm/glm.hpp>
#include <glm/include/glm/gtc/matrix_transform.hpp>
#include <glm/include/glm/gtx/euler_angles.hpp>

#include "RootSignature.h"
#include "Shader.h"
#include "StagingBuffer.h"
#include "Utils.h"

Texture Texture::CreateTexture(Microsoft::WRL::ComPtr<ID3D12Device> m_Device,DescriptorHeap& m_DescHeapCBV_SRV_UAV,UINT Width,UINT Height,UINT Depth,DXGI_FORMAT Format,UINT Levels)
{
    assert(Depth == 1 || Depth == 6);

    Texture texture;
    texture.Width = Width;
    texture.Height = Height;
    texture.Levels = (Levels > 0) ? Levels : Utils::numMipmapLevels(Width, Height);

    D3D12_RESOURCE_DESC Desc = {};
    Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.DepthOrArraySize = Depth;
    Desc.MipLevels = Levels;
    Desc.Format = Format;
    Desc.SampleDesc.Count = 1;
    Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if(Format == DXGI_FORMAT_R24G8_TYPELESS)
    {
        D3D12_CLEAR_VALUE OptClear;
        OptClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        OptClear.DepthStencil.Depth = 1.0f;
        OptClear.DepthStencil.Stencil = 0;

        auto DefaultType = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
        if (FAILED(m_Device->CreateCommittedResource(
            &DefaultType,
            D3D12_HEAP_FLAG_NONE,
            &Desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&texture.texture))))
        {
            throw std::runtime_error("Failed to create 2D texture");
        }

        D3D12_SRV_DIMENSION SrvDim;
        switch (Depth)
        {
            case 1: SrvDim = D3D12_SRV_DIMENSION_TEXTURE2D; break;
            case 6:SrvDim = D3D12_SRV_DIMENSION_TEXTURECUBE; break;
            default:SrvDim = D3D12_SRV_DIMENSION_TEXTURE2DARRAY; break;
        }

        CreateTextureSRV(m_Device, m_DescHeapCBV_SRV_UAV, texture, SrvDim);

        return texture;
    }

    auto DefaultType = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
    if (FAILED(m_Device->CreateCommittedResource(
        &DefaultType,
        D3D12_HEAP_FLAG_NONE,
        &Desc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&texture.texture))))
    {
        throw std::runtime_error("Failed to create 2D texture");
    }

    D3D12_SRV_DIMENSION SrvDim;
    switch (Depth)
    {
    case 1: SrvDim = D3D12_SRV_DIMENSION_TEXTURE2D; break;
    case 6:SrvDim = D3D12_SRV_DIMENSION_TEXTURECUBE; break;
    default:SrvDim = D3D12_SRV_DIMENSION_TEXTURE2DARRAY; break;
    }
    CreateTextureSRV(m_Device, m_DescHeapCBV_SRV_UAV,texture, SrvDim);

    return texture;
}

Texture Texture::CreateTexture(
    std::function<void()> CallBack,
    Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList,
    MipMapGeneration& m_mipmapGeneration,
    DescriptorHeap& m_DescHeapCBV_SRV_UAV,
    D3D_ROOT_SIGNATURE_VERSION& m_RootSignatureVersion,
    const std::shared_ptr<class Image>& Image, DXGI_FORMAT Format,
    UINT Levels)
{
    Texture texture = CreateTexture(m_Device, m_DescHeapCBV_SRV_UAV,Image->Width(), Image->Height(), 1, Format, Levels);
    StagingBuffer TextureStagingBuffer;
    {
        const D3D12_SUBRESOURCE_DATA Data{ Image->Pixels<void>(),Image->Pitch() };
        TextureStagingBuffer = StagingBuffer::CreateStagingBuffer(m_Device,texture.texture, 0, 1, &Data);
    }

    {
        const CD3DX12_TEXTURE_COPY_LOCATION DestCopyLocation{ texture.texture.Get(),0 };
        const CD3DX12_TEXTURE_COPY_LOCATION SrcCopyLocation{ TextureStagingBuffer.Buffer.Get(),TextureStagingBuffer.Layouts[0] };

        auto Common2Dest = CD3DX12_RESOURCE_BARRIER::Transition(texture.texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST, 0);

        auto Dest2Common = CD3DX12_RESOURCE_BARRIER::Transition(texture.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON, 0);

        m_CommandList->ResourceBarrier(1, &Common2Dest);
        m_CommandList->CopyTextureRegion(&DestCopyLocation, 0, 0, 0, &SrcCopyLocation, nullptr);
        m_CommandList->ResourceBarrier(1, &Dest2Common);
    }

    if (texture.Levels > 1 && texture.Width == texture.Height && Utils::IsPowerOfTwo(texture.Width))
    {
        GenerateMipmaps(CallBack,m_Device,m_CommandList, m_mipmapGeneration,m_DescHeapCBV_SRV_UAV,m_RootSignatureVersion,texture);
    }
    else {
        // Only execute & wait if not generating mipmaps (so that we don't wait redundantly).
        CallBack();
        // ExecuteCommandList();
        // WaitForGPU();
    }

    return texture;
}

void Texture::GenerateMipmaps(std::function<void()> Callback, Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList,
    MipMapGeneration& m_mipmapGeneration,
    DescriptorHeap& m_DescHeapCBV_SRV_UAV,
    D3D_ROOT_SIGNATURE_VERSION& m_RootSignatureVersion,
    const Texture& texture)
{
    assert(texture.Width == texture.Height);
    assert(Utils::IsPowerOfTwo(texture.Width));

    if (!m_mipmapGeneration.RootSignature)
    {
        const CD3DX12_DESCRIPTOR_RANGE1 descriptorRanges[] = {
            {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE},
            {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE},
        };
        CD3DX12_ROOT_PARAMETER1 rootParameters[2];
        rootParameters[0].InitAsDescriptorTable(1, &descriptorRanges[0]);
        rootParameters[1].InitAsDescriptorTable(1, &descriptorRanges[1]);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(2, rootParameters);
        m_mipmapGeneration.RootSignature = RootSignature::CreateRootSignature(m_Device, m_RootSignatureVersion,rootSignatureDesc);
    }

    ID3D12PipelineState* pipelineState = nullptr;

    const D3D12_RESOURCE_DESC desc = texture.texture->GetDesc();
    if (desc.DepthOrArraySize == 1 && desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
        if (!m_mipmapGeneration.gammaTexturePipelineState)
        {
            Microsoft::WRL::ComPtr<ID3DBlob> computeShader = Shader::compileShader(
                "shaders/hlsl/downsample.hlsl",
                "downsample_gamma",
                "cs_5_0");

            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = m_mipmapGeneration.RootSignature.Get();
            psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());

            if (FAILED(m_Device->CreateComputePipelineState(
                &psoDesc,
                IID_PPV_ARGS(&m_mipmapGeneration.gammaTexturePipelineState))))
            {
                throw std::runtime_error("Failed to create compute pipeline state (gamma correct downsample filter)");
            }
        }
        pipelineState = m_mipmapGeneration.gammaTexturePipelineState.Get();
    }
    else if (desc.DepthOrArraySize > 1 && desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
    {
        if (!m_mipmapGeneration.ArrayTexturePipelineState)
        {
            Microsoft::WRL::ComPtr<ID3DBlob> computeShader = Shader::compileShader(
                "shaders/hlsl/downsample_array.hlsl",
                "downsample_linear",
                "cs_5_0");
            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = m_mipmapGeneration.RootSignature.Get();
            psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());
            if (FAILED(m_Device->CreateComputePipelineState(
                &psoDesc,
                IID_PPV_ARGS(&m_mipmapGeneration.ArrayTexturePipelineState))))
            {
                throw std::runtime_error("Failed to create compute pipeline state (array downsample filter)");
            }
        }
        pipelineState = m_mipmapGeneration.ArrayTexturePipelineState.Get();
    }
    else {
        assert(desc.DepthOrArraySize == 1);
        if (!m_mipmapGeneration.LinearTexturePipelineState)
        {
            Microsoft::WRL::ComPtr<ID3DBlob> computeShader = Shader::compileShader(
                "shaders/hlsl/downsample.hlsl",
                "downsample_linear",
                "cs_5_0");
            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = m_mipmapGeneration.RootSignature.Get();
            psoDesc.CS = CD3DX12_SHADER_BYTECODE(computeShader.Get());
            if (FAILED(m_Device->CreateComputePipelineState(
                &psoDesc,
                IID_PPV_ARGS(&m_mipmapGeneration.LinearTexturePipelineState))))
            {
                throw std::runtime_error("Failed to create compute pipeline state (linear downsample filter)");
            }
        }
        pipelineState = m_mipmapGeneration.LinearTexturePipelineState.Get();
    }

    DescriptorHeapMark mark(m_DescHeapCBV_SRV_UAV);

    Texture linearTexture = texture;
    if (desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
    {
        pipelineState = m_mipmapGeneration.gammaTexturePipelineState.Get();
        linearTexture = CreateTexture(
            m_Device,
            m_DescHeapCBV_SRV_UAV,
            texture.Width,
            texture.Height,
            1,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            texture.Levels);

        auto Common2Dest = CD3DX12_RESOURCE_BARRIER::Transition(linearTexture.texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        auto Dest2Common = CD3DX12_RESOURCE_BARRIER::Transition(linearTexture.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

        m_CommandList->ResourceBarrier(1, &Common2Dest);
        m_CommandList->CopyResource(linearTexture.texture.Get(), texture.texture.Get());
        m_CommandList->ResourceBarrier(1, &Dest2Common);
    }

    ID3D12DescriptorHeap* descriptorHeaps[] = {
        m_DescHeapCBV_SRV_UAV.Heap.Get()
    };

    m_CommandList->SetComputeRootSignature(m_mipmapGeneration.RootSignature.Get());
    m_CommandList->SetDescriptorHeaps(1, descriptorHeaps);
    m_CommandList->SetPipelineState(pipelineState);

    std::vector<CD3DX12_RESOURCE_BARRIER> preDispatchBarriers{ desc.DepthOrArraySize };
    std::vector<CD3DX12_RESOURCE_BARRIER> postDispatchBarriers{ desc.DepthOrArraySize };
    for (UINT level = 1, levelWidth = texture.Width / 2, levelHeight = texture.Height / 2; level < texture.Levels; ++level, levelWidth /= 2, levelHeight /= 2)
    {
        CreateTextureSRV(
            m_Device,
            m_DescHeapCBV_SRV_UAV,
            linearTexture,
            desc.DepthOrArraySize > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY : D3D12_SRV_DIMENSION_TEXTURE2D,
            level - 1,
            1);

        CreateTextureUAV(m_Device,m_DescHeapCBV_SRV_UAV,linearTexture, level);

        for (UINT arraySlice = 0; arraySlice < desc.DepthOrArraySize; ++arraySlice)
        {
            const UINT subresourceIndex = D3D12CalcSubresource(
                level,
                arraySlice,
                0,
                texture.Levels,
                desc.DepthOrArraySize);

            preDispatchBarriers[arraySlice] = CD3DX12_RESOURCE_BARRIER::Transition(linearTexture.texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, subresourceIndex);

            postDispatchBarriers[arraySlice] = CD3DX12_RESOURCE_BARRIER::Transition(linearTexture.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON, subresourceIndex);
        }

        m_CommandList->ResourceBarrier(desc.DepthOrArraySize, preDispatchBarriers.data());
        m_CommandList->SetComputeRootDescriptorTable(0, linearTexture.Srv.GpuHandle);
        m_CommandList->SetComputeRootDescriptorTable(1, linearTexture.Uav.GpuHandle);
        m_CommandList->Dispatch(glm::max(UINT(1), levelWidth / 8), glm::max(UINT(1), levelHeight / 8), desc.DepthOrArraySize);
        m_CommandList->ResourceBarrier(desc.DepthOrArraySize, postDispatchBarriers.data());
    }

    auto Non2Common = CD3DX12_RESOURCE_BARRIER::Transition(texture.texture.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COMMON);

    auto Source2Dest = CD3DX12_RESOURCE_BARRIER::Transition(texture.texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

    auto Dest2Source = CD3DX12_RESOURCE_BARRIER::Transition(texture.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);


    if (texture.texture == linearTexture.texture) {
        m_CommandList->ResourceBarrier(1, &Non2Common);
    }
    else {
        m_CommandList->ResourceBarrier(1, &Source2Dest);
        m_CommandList->CopyResource(texture.texture.Get(), linearTexture.texture.Get());
        m_CommandList->ResourceBarrier(1, &Dest2Source);
    }
    
    Callback();
}

void Texture::CreateTextureSRV(Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
    DescriptorHeap& m_DescHeapCBV_SRV_UAV,
    Texture& texture,
    D3D12_SRV_DIMENSION Dimension,
    UINT MostDetailedMip,
    UINT MipLevels)
{
    const D3D12_RESOURCE_DESC Desc = texture.texture->GetDesc();
    const UINT EffectiveMipLevels = (MipLevels > 0) ? MipLevels : (Desc.MipLevels - MostDetailedMip);

    assert(!(Desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE));
    texture.Srv = m_DescHeapCBV_SRV_UAV.Alloc();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = Desc.Format;
    srvDesc.ViewDimension = Dimension;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    switch (Dimension)
    {
    case D3D12_SRV_DIMENSION_TEXTURE2D:
        srvDesc.Texture2D.MostDetailedMip = MostDetailedMip;
        srvDesc.Texture2D.MipLevels = EffectiveMipLevels;
        break;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
        srvDesc.Texture2DArray.MostDetailedMip = MostDetailedMip;
        srvDesc.Texture2DArray.MipLevels = EffectiveMipLevels;
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize = Desc.DepthOrArraySize;
        break;
    case D3D12_SRV_DIMENSION_TEXTURECUBE:
        assert(Desc.DepthOrArraySize == 6);
        srvDesc.TextureCube.MostDetailedMip = MostDetailedMip;
        srvDesc.TextureCube.MipLevels = EffectiveMipLevels;
        break;
    default:
        assert(0);
    }
    m_Device->CreateShaderResourceView(texture.texture.Get(), &srvDesc, texture.Srv.CpuHandle);
}

void Texture::CreateTextureUAV(Microsoft::WRL::ComPtr<ID3D12Device> m_Device,
    DescriptorHeap& m_DescHeapCBV_SRV_UAV,
    Texture& texture,
    UINT mipSlice)
{
    const D3D12_RESOURCE_DESC desc = texture.texture->GetDesc();
    assert(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    texture.Uav = m_DescHeapCBV_SRV_UAV.Alloc();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = desc.Format;
    if (desc.DepthOrArraySize > 1) {
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        uavDesc.Texture2DArray.MipSlice = mipSlice;
        uavDesc.Texture2DArray.FirstArraySlice = 0;
        uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
    }
    else {
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Texture2D.MipSlice = mipSlice;
    }
    m_Device->CreateUnorderedAccessView(texture.texture.Get(), nullptr, &uavDesc, texture.Uav.CpuHandle);
}
