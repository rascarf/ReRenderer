#pragma once
#include "GeometryGenerator.h"
#include "MeshBuffer.h"
#include "RootSignature.h"
#include <functional>


class Debugger
{
public:
    MeshBuffer QuadBuffer;
    ComPtr<ID3D12RootSignature> m_DebugRootSignature;
    ComPtr<ID3D12PipelineState> m_DebugPso;
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;

    ComPtr<ID3D12Device> m_Device;
    CD3DX12_STATIC_SAMPLER_DESC& m_DefaultSamplerDesc;
    D3D_ROOT_SIGNATURE_VERSION& m_RootSignatureVersion;
    std::function<void()> m_CallBack;

    Texture& DebugTexture;

    Debugger(
        std::function<void()> m_CallBack,
        ComPtr<ID3D12Device> Device, 
        ComPtr<ID3D12GraphicsCommandList> CommandList, const std::vector<D3D12_INPUT_ELEMENT_DESC> Layout,
        CD3DX12_STATIC_SAMPLER_DESC& DefaultSamplerDesc,
        D3D_ROOT_SIGNATURE_VERSION& RootSignatureVersion, 
        UINT SampleCount,
        Texture& DebugTexture,
        float x,float y , float w,float h,float depth
    );

    void Draw();

};

