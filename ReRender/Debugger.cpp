#include "Debugger.h"

#include "Shader.h"

Debugger::Debugger(
    std::function<void()> CallBack,
    ComPtr<ID3D12Device> Device,
    ComPtr<ID3D12GraphicsCommandList> CommandList,
    const std::vector<D3D12_INPUT_ELEMENT_DESC> Layout,
    CD3DX12_STATIC_SAMPLER_DESC& DefaultSamplerDesc,
    D3D_ROOT_SIGNATURE_VERSION& RootSignatureVersion,
    UINT SampleCount,
    Texture& InDebugTexture,
    float x, float y, float w, float h, float depth):
    m_Device(Device),
    m_DefaultSamplerDesc(DefaultSamplerDesc),
    m_RootSignatureVersion(RootSignatureVersion),
    m_CommandList(CommandList),
    DebugTexture(InDebugTexture)
{
    //创建MeshBuffer
    {
        MeshData quad = GeometryGenerator::CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
        QuadBuffer = MeshBuffer::CreateMeshBuffer(CallBack, m_CommandList, m_Device, quad);
        SetDebugName(QuadBuffer.VertexBuffer.Get(),QuadBuffer)
    }

    //创建ShadowMap的根签名和PSO
    {
        ComPtr<ID3DBlob> DebugVS = Shader::compileShader(
            "shaders/hlsl/debug.hlsl",
            "main_vs",
            "vs_5_0"
        );

        ComPtr<ID3DBlob> DebugPS = Shader::compileShader(
            "shaders/hlsl/debug.hlsl",
            "main_ps",
            "ps_5_0"
        );

        const CD3DX12_DESCRIPTOR_RANGE1 DescriptorRanges[] =
        {
            {
                    D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
                }
        };

        CD3DX12_ROOT_PARAMETER1 root_parameter[1];
        root_parameter[0].InitAsDescriptorTable(1, &DescriptorRanges[0], D3D12_SHADER_VISIBILITY_ALL);

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC SignatureDesc = {};
        SignatureDesc.Init_1_1(1, root_parameter, 1, &m_DefaultSamplerDesc,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        m_DebugRootSignature = RootSignature::CreateRootSignature(m_Device, m_RootSignatureVersion, SignatureDesc);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_DebugRootSignature.Get();
        psoDesc.InputLayout = { Layout.data(), (UINT)Layout.size() };
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(DebugVS.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(DebugPS.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = false;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        psoDesc.SampleDesc.Count = 8;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        if (FAILED(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_DebugPso)))) {
            throw std::runtime_error("Failed to create graphics pipeline state for Debbuger");
        }
    }

    Re::SetName(m_DebugRootSignature.Get(), std::string("DebugSignature").c_str());
    Re::SetName(m_DebugPso.Get(), std::string("m_DebugPSO").c_str());

}

void Debugger::Draw()
{
    m_CommandList->SetGraphicsRootSignature(m_DebugRootSignature.Get());
    m_CommandList->SetGraphicsRootDescriptorTable(0, DebugTexture.Srv.GpuHandle);
    m_CommandList->SetPipelineState(m_DebugPso.Get());
    m_CommandList->IASetVertexBuffers(0, 1, &QuadBuffer.Vbv);
    m_CommandList->IASetIndexBuffer(&QuadBuffer.Ibv);

    m_CommandList->DrawIndexedInstanced(QuadBuffer.NumElements, 1, 0, 0, 0);

}
