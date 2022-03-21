#pragma once
#include <memory>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdexcept>
#include <d3dx12/d3dx12.h>
#include <wrl/client.h>

#include "Descriptor.h"
#include "renderer.h"
#include "StagingBuffer.h"
#include "Texture.h"
#include "utils.h"

class RootSignature
{
public:
    static Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(Microsoft::WRL::ComPtr<ID3D12Device> m_Device,D3D_ROOT_SIGNATURE_VERSION m_RootSignatureVersion,D3D12_VERSIONED_ROOT_SIGNATURE_DESC& Desc)
    {
        const D3D12_ROOT_SIGNATURE_FLAGS StandardFlags =
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

        switch (Desc.Version)
        {
        case D3D_ROOT_SIGNATURE_VERSION_1_0:
            Desc.Desc_1_0.Flags |= StandardFlags;
            break;
        case D3D_ROOT_SIGNATURE_VERSION_1_1:
            Desc.Desc_1_1.Flags |= StandardFlags;
            break;
        }

        Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignature;
        Microsoft::WRL::ComPtr<ID3DBlob> SignatureBlob, ErrorBlob;

        if (FAILED(D3DX12SerializeVersionedRootSignature(
            &Desc,
            m_RootSignatureVersion,
            &SignatureBlob,
            &ErrorBlob)))
        {
            throw std::runtime_error("Failed to serialize root signature");
        }

        if (FAILED(m_Device->CreateRootSignature(
            0,
            SignatureBlob->GetBufferPointer(),
            SignatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&RootSignature))))
        {
            throw std::runtime_error("Failed to create root signature");
        }
        return RootSignature;
    }
};

