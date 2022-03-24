#pragma once
#include <d3dx12/d3dx12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include "Camera.h"
#include "renderer.h"
#include "Texture.h"

using Microsoft::WRL::ComPtr;
using Vec2 = glm::vec2;

struct Constant4Shader
{
    Mat4 Light2Tetxure;
    float NearZ = 0.0f;
    float FarZ = 0.0f;
    Vec2 RenderTargetSize = { 0.0f,0.0f};
};

class ShadowMap
{
public:
    ShadowMap(
        ComPtr<ID3D12Device> m_Device,
        DescriptorHeap& m_DescHeapCBV_SRV_UAV,
        DescriptorHeap& m_DescHeapDsv,
        UINT Width,
        UINT Height,
        UINT SamperCount,
        const std::vector<D3D12_INPUT_ELEMENT_DESC> Layout,
        CD3DX12_STATIC_SAMPLER_DESC& DefaultSamplerDesc,
        D3D_ROOT_SIGNATURE_VERSION& RootSignatureVersion);

    void UpdateShadowTransform(const float DeltaTime,Light InLight);
    void UpdateShadowConstantBuffer();


private:

    ComPtr<ID3D12Device> m_Device;
    ComPtr<ID3D12RootSignature> m_ShadowSignature;
    ComPtr<ID3D12PipelineState> m_ShadowPipelineState;

    DescriptorHeap& m_DescHeapCBV_SRV_UAV;
    DescriptorHeap& m_DescHeapDsv;

    Descriptor Dsv;

    CD3DX12_STATIC_SAMPLER_DESC& m_DefaultSamplerDesc;
    D3D_ROOT_SIGNATURE_VERSION& m_RootSignatureVersion;

    int Width;
    int Height;

    Texture ShadowMapTexture;

    CD3DX12_VIEWPORT m_Viewport;
    CD3DX12_RECT m_Rect;

    Vec3 m_LightPosition;
    Vec3 m_LightDirection;

    Mat4 m_LightView;
    Mat4 m_LightProject;
    Mat4 m_LightToTexture;

    Constant4Shader CpuConstant;
};

