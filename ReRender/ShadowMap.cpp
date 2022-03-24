#include "ShadowMap.h"

#include <glm/include/glm/gtc/matrix_transform.hpp>

#include "Camera.h"
#include "RootSignature.h"
#include "Shader.h"

ShadowMap::ShadowMap(
    ComPtr<ID3D12Device> Device, 
    DescriptorHeap& InDescHeapCBV_SRV_UAV,
    DescriptorHeap& InDescHeapDsv,
    UINT InWidth, UINT InHeight,
    UINT SamperCount,
    const std::vector<D3D12_INPUT_ELEMENT_DESC> Layout,
    CD3DX12_STATIC_SAMPLER_DESC& DefaultSamplerDesc,
    D3D_ROOT_SIGNATURE_VERSION& RootSignatureVersion):
    m_Device(Device),
    m_DescHeapCBV_SRV_UAV(InDescHeapCBV_SRV_UAV),
    m_DescHeapDsv(InDescHeapDsv),
    Width(InWidth),
    Height(InHeight),
    m_DefaultSamplerDesc(DefaultSamplerDesc),
    m_RootSignatureVersion(RootSignatureVersion)

{
    DescriptorHeapMark mark(m_DescHeapCBV_SRV_UAV);

    ShadowMapTexture = Texture::CreateTexture(m_Device, m_DescHeapCBV_SRV_UAV, Width, Height, 1, DXGI_FORMAT_R24G8_TYPELESS, 1,D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    //创建ShadowMap的根签名和PSO
    {
        ComPtr<ID3DBlob> ShadowMapVS = Shader::compileShader(
            "shaders/hlsl/ShadowMap.hlsl",
            "main_vs",
            "vs_5_0"
            );

        ComPtr<ID3DBlob> ShadowMapPS = Shader::compileShader(
            "shaders/hlsl/ShadowMap.hlsl",
            "main_ps",
            "ps_5_0"
            );

        const CD3DX12_DESCRIPTOR_RANGE1 DescriptorRanges[] =
        {
            {
                D3D12_DESCRIPTOR_RANGE_TYPE_CBV,1,0,0,
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC
            }
        };

        CD3DX12_ROOT_PARAMETER1 root_parameter[1];
        root_parameter[0].InitAsDescriptorTable(1, &DescriptorRanges[0], D3D12_SHADER_VISIBILITY_VERTEX);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC SignatureDesc = {};
        SignatureDesc.Init_1_1(1, root_parameter, 1, &m_DefaultSamplerDesc,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        m_ShadowSignature = RootSignature::CreateRootSignature(m_Device, m_RootSignatureVersion, SignatureDesc);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_ShadowSignature.Get();
        psoDesc.InputLayout = { Layout.data(), (UINT)Layout.size() };
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(ShadowMapVS.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(ShadowMapPS.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.RasterizerState.DepthBias = 100000;
        psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
        psoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
        psoDesc.SampleDesc.Count = SamperCount;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        if (FAILED(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_ShadowPipelineState)))) {
            throw std::runtime_error("Failed to create graphics pipeline state for skybox");
        }
    }

    //视口
     m_Viewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, (FLOAT)Width, (FLOAT)Height };

     m_Rect = CD3DX12_RECT{ 0, 0, (LONG)Width, (LONG)Height };

    //Texture要做深度图使用，额外创建一个DSV
     Dsv = m_DescHeapDsv.Alloc();

     D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
     dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
     dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
     dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
     dsvDesc.Texture2D.MipSlice = 0;
     m_Device->CreateDepthStencilView(ShadowMapTexture.texture.Get(), &dsvDesc, Dsv.CpuHandle);
}

void ShadowMap::UpdateShadowTransform(const float DeltaTime,Light InLight, ConstantBufferView ConstantBuffer)
{
    m_LightPosition = InLight.Position;
    m_LightDirection = InLight.Direction;
    auto LightUp = Vec3{ 0.0f,1.0f,0.0f };

    m_LightView = glm::lookAt(m_LightPosition, m_LightPosition + m_LightDirection, LightUp);

    m_LightProject = glm::orthoRH_ZO(-100.0f,100.0f,-100.0f,100.0f,0.0f,1000.0f);

    //构建NDCToTexture
    Mat4 T
    { 
        0.5f , 0.0f ,0.0f , 0.5f,
        0.0f , 0.5f , 0.0f , 0.5f,
        0.0f , 0.0f , 1.0f , 0.0f,
        0.0f , 0.0f , 0.0f , 1.0f
    };

    //LightToTexture
    m_LightToTexture = T * m_LightProject * m_LightView;

    UpdateShadowConstantBuffer(ConstantBuffer);
}

void ShadowMap::UpdateShadowConstantBuffer(ConstantBufferView ConstantBuffer)
{
    CpuConstant.Light2Tetxure = m_LightToTexture;
    CpuConstant.NearZ = 0.1f;
    CpuConstant.FarZ = 1000.0f;
    CpuConstant.RenderTargetSize = { 1024.0f,1024.0f };

    Constant4Shader* ShadowConstantInBuffer = ConstantBuffer.as<Constant4Shader>();
    ShadowConstantInBuffer->FarZ = CpuConstant.FarZ;
    ShadowConstantInBuffer->NearZ = CpuConstant.NearZ;
    ShadowConstantInBuffer->Light2Tetxure = CpuConstant.Light2Tetxure;
    ShadowConstantInBuffer->RenderTargetSize = CpuConstant.RenderTargetSize;
}
