#include "D3D12Renderer.h"

#include <algorithm>
#include <stdexcept>

#include "Mesh.h"
#include "Image.h"

#include <d3dx12/d3dx12.h>
#include <d3dcompiler.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <glfw/include/GLFW/glfw3.h>
#include <glfw/include/GLFW/glfw3native.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/include/glm/glm.hpp>
#include <glm/include/glm/gtc/matrix_transform.hpp>
#include <glm/include/glm/gtx/euler_angles.hpp>



using Mat4 = glm::mat4;
using Vec4 = glm::vec4;
using Vec3 = glm::vec3;


struct TransformCB
{
    Mat4 ViewPorjectionMatrix;
    Mat4 SkyProjectionMatrix;
    Mat4 SceneRotationMatix;
};

struct ShadingCB
{
    struct 
    {
        Vec4 Direction;
        Vec4 Radiance;
    }Light[SceneSettings::NumLights];

    Vec4 EyePosition;
};

GLFWwindow* D3D12Renderer::initialize(int Width, int Height, int MaxSamples)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* Window = glfwCreateWindow(Width, Height, "RaRenderer", nullptr, nullptr);

    if(!Window)
    {
        throw std::runtime_error("Failed to create window");
    }

    UINT DxgiFactoryFlags = 0;

#ifdef _DEBUG
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
            DxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> DxgiFactory;
    if (FAILED(CreateDXGIFactory2(DxgiFactoryFlags, IID_PPV_ARGS(&DxgiFactory))))
    {
        throw std::runtime_error("Failed to create DXGI factory");
    }

    std::string DxgiAdapterDescription;
    {
        ComPtr<IDXGIAdapter1> Adapter = getAdapter(DxgiFactory);
        if(Adapter)
        {
            DXGI_ADAPTER_DESC AdapterDesc;
            Adapter->GetDesc(&AdapterDesc);
            DxgiAdapterDescription = Utils::convertToUTF8(AdapterDesc.Description);
        }
        else
        {
            throw std::runtime_error("No Suitable video adapter");
        }

        if(FAILED(D3D12CreateDevice(Adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&m_Device))))
        {
            throw std::runtime_error("Failed to create D3D12 device");
        }
    }

    //创建默认Command queue
    D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
    QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    if(FAILED(m_Device->CreateCommandQueue(&QueueDesc,IID_PPV_ARGS(&m_CommandQueue))))
    {
        throw std::runtime_error("Failed to create command queue");
    }

    //创建交换链
    {
        DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = {  };
        SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.SampleDesc.Count = 1;
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.BufferCount = NumFrames;
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        ComPtr<IDXGISwapChain1> SwapChain;
        if (FAILED(DxgiFactory->CreateSwapChainForHwnd
        (
            m_CommandQueue.Get(), 
            glfwGetWin32Window(Window), 
            &SwapChainDesc, 
            nullptr, 
            nullptr, 
            &SwapChain)))
        {
            throw std::runtime_error("Failed to create swap chain");
        }
        SwapChain.As(&m_SwapChain);
    }

    m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
    DxgiFactory->MakeWindowAssociation(glfwGetWin32Window(Window),DXGI_MWA_NO_ALT_ENTER);

    //确定最大的MSAA level
    UINT Samples;
    for(Samples = MaxSamples;Samples > 1; Samples /= 2)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS mqlColor = { DXGI_FORMAT_R16G16B16A16_FLOAT, Samples };
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS mqlDepthStencil = { DXGI_FORMAT_D24_UNORM_S8_UINT, Samples };
        m_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &mqlColor, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS));
        m_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &mqlDepthStencil, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS));
        if (mqlColor.NumQualityLevels > 0 && mqlDepthStencil.NumQualityLevels > 0) 
        {
            break;
        }
    }

    //确定根签名版本
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSignatureFeature = { D3D_ROOT_SIGNATURE_VERSION_1_1 };
        m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSignatureFeature, sizeof(D3D12_FEATURE_DATA_ROOT_SIGNATURE));
        m_RootSignatureVersion = rootSignatureFeature.HighestVersion;
    }

    //创建描述符堆
    {
        m_DescHeapRtv = CreateDescriptorHeap(
            { D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                16,
                D3D12_DESCRIPTOR_HEAP_FLAG_NONE });

        m_DescHeapDsv = CreateDescriptorHeap(
            {
                D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                16,
            D3D12_DESCRIPTOR_HEAP_FLAG_NONE });

        m_DescHeapCBV_SRV_UAV = CreateDescriptorHeap(
            {
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                128,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE });
    }

    //创建每帧的资源
    for (UINT FrameIndex = 0; FrameIndex < NumFrames; ++FrameIndex)
    {
        if (FAILED(m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_CommandAllocators[FrameIndex]))))
        {
            throw std::runtime_error("Failed to create command allocator");
        }

        if (FAILED(m_SwapChain->GetBuffer(FrameIndex, IID_PPV_ARGS(&m_BackBuffers[FrameIndex].Buffer))))
        {
            throw std::runtime_error("Failed to retrieve swap chain back buffer");
        }

        m_BackBuffers[FrameIndex].Rtv = m_DescHeapRtv.Alloc();
        m_Device->CreateRenderTargetView(
            m_BackBuffers[FrameIndex].Buffer.Get(),
            nullptr,
            m_BackBuffers[FrameIndex].Rtv.CpuHandle
        );

        m_FrameBuffers[FrameIndex] = CreateFrameBuffer(
            Width,
            Height,
            Samples,
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_D24_UNORM_S8_UINT
        );

        if (Samples > 1)
        {
            m_ResolveFrameBuffers[FrameIndex] = CreateFrameBuffer(
                Width,
                Height,
                1,
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                (DXGI_FORMAT)0
            );
        }
        else
        {
            m_ResolveFrameBuffers[FrameIndex] = m_FrameBuffers[FrameIndex];
        }
    }

    //创建Fence
    {
        if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence)))) 
        {
            throw std::runtime_error("Failed to create fence object");
        }
        m_FenceCompletionEvent = CreateEvent(nullptr, false, false, nullptr);
    }

    std::printf("Direct3D 12 Renderer [%s]\n", DxgiAdapterDescription.c_str());

    return Window;
    
}

void D3D12Renderer::ShutDown()
{
    WaitForGPU();
    CloseHandle(m_FenceCompletionEvent);
}

void D3D12Renderer::Setup()
{
    CD3DX12_STATIC_SAMPLER_DESC DefaultSamplerDesc{
    0,
    D3D12_FILTER_ANISOTROPIC
    };
    DefaultSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_STATIC_SAMPLER_DESC ComputeSamplerDesc
    {
        0,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR
    };
    DefaultSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_STATIC_SAMPLER_DESC spBRDF_SamplerDesc
    {
        1,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR
    };
    spBRDF_SamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    spBRDF_SamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    spBRDF_SamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    //创建CommandList
    if (FAILED(m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_CommandList)))) 
    {
        throw std::runtime_error("Failed to create direct command list");
    }

    //创建Tonemap的根签名和PSO
    {
        ComPtr<ID3DBlob> ToneMapVs = compileShader(
            "Shaders/hlsl/tonemap.hlsl",
            "main_vs",
            "vs_5_0");

        ComPtr<ID3DBlob> ToneMapPs = compileShader(
            "Shaders/hlsl/tonemap.hlsl",
            "main_ps",
            "ps_5_0");

        const CD3DX12_DESCRIPTOR_RANGE1 DescriptorRanges[] =
        {
           { D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            1,
            0,
               0,
            D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE
            },
        };

        CD3DX12_ROOT_PARAMETER1 RootParameters[1];
        RootParameters[0].InitAsDescriptorTable(
            1,
            &DescriptorRanges[0],
            D3D12_SHADER_VISIBILITY_PIXEL
        );

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC SignatureDesc = {};
        SignatureDesc.Init_1_1(
            1,
            RootParameters,
            1,
            &ComputeSamplerDesc,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS);

        m_ToneMapRootSignature = CreateRootSignature(SignatureDesc);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC PsoDesc = {};
        PsoDesc.pRootSignature = m_ToneMapRootSignature.Get();
        PsoDesc.VS = CD3DX12_SHADER_BYTECODE(ToneMapVs.Get());
        PsoDesc.PS = CD3DX12_SHADER_BYTECODE(ToneMapPs.Get());
        PsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT };
        PsoDesc.RasterizerState.FrontCounterClockwise = true;
        PsoDesc.BlendState = CD3DX12_BLEND_DESC{ D3D12_DEFAULT };
        PsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        PsoDesc.NumRenderTargets = 1;
        PsoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        PsoDesc.SampleDesc.Count = 1;
        PsoDesc.SampleMask = UINT_MAX;

        if (FAILED(m_Device->CreateGraphicsPipelineState(
            &PsoDesc, 
            IID_PPV_ARGS(&m_ToneMapPipelineState))))
        {
            throw std::runtime_error("Failed to create tonemap pipeline state");
        }
    }

    //创建PBR model的根签名和PSO
    {
        const std::vector<D3D12_INPUT_ELEMENT_DESC> MeshInputLayout =
        {
            {
                "POSITION",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "NORMAL",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                12,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "TANGENT",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                24,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "BITANGENT",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                36,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "TEXCOORD",
                0,
                DXGI_FORMAT_R32G32_FLOAT,
                0,
                48,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            }
        };

        ComPtr<ID3DBlob> PbrVs = compileShader(
            "Shaders/hlsl/pbr.hlsl",
            "main_vs",
            "vs_5_0");

        ComPtr<ID3DBlob> PbrPs = compileShader(
            "shaders/hlsl/pbr.hlsl",
            "main_ps",
            "ps_5_0"
        );

        const CD3DX12_DESCRIPTOR_RANGE1 DescriptorRange[] =
        {
            {
                D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
                1,
                0,
                0,
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC
            },
            {
                D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
                7,
                0,
                0,
                D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC
            }
        };

        CD3DX12_ROOT_PARAMETER1 RootParameter[3];
        RootParameter[0].InitAsDescriptorTable(
            1,
            &DescriptorRange[0],
            D3D12_SHADER_VISIBILITY_VERTEX
        );
        RootParameter[1].InitAsDescriptorTable(
            1,
            &DescriptorRange[0],
            D3D12_SHADER_VISIBILITY_PIXEL
        );
        RootParameter[2].InitAsDescriptorTable(
            1,
            &DescriptorRange[1],
            D3D12_SHADER_VISIBILITY_PIXEL
        );

        D3D12_STATIC_SAMPLER_DESC StaticSamplers[2];
        StaticSamplers[0] = DefaultSamplerDesc;
        StaticSamplers[1] = spBRDF_SamplerDesc;


        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC SignatureDesc;
        SignatureDesc.Init_1_1(
            3,
            RootParameter,
            2,
            StaticSamplers,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        );

        m_PbrRootSignature = CreateRootSignature(SignatureDesc);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_PbrRootSignature.Get();
        psoDesc.InputLayout = { MeshInputLayout.data(), (UINT)MeshInputLayout.size() };
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(PbrVs.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(PbrPs.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleDesc.Count = m_FrameBuffers[0].Samples;
        psoDesc.SampleMask = UINT_MAX;

        if (FAILED(m_Device->CreateGraphicsPipelineState(
            &psoDesc, 
            IID_PPV_ARGS(&m_PbrPipelineState)))) 
        {
            throw std::runtime_error("Failed to create graphics pipeline state for PBR model");
        }
    }

    //加载PBRasset
    {
        m_PbrModel = CreateMeshBuffer(Mesh::FromFile("Meshes/cerberus.fbx"));
        m_AlbedoTexture = CreateTexture(
            Image::FromFile("textures/cerberus_A.png"),
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        m_NormalTexture = CreateTexture(
            Image::FromFile("textures/cerberus_N.png"),
            DXGI_FORMAT_R8G8B8A8_UNORM);
        m_MetalnessTexture = CreateTexture(
            Image::FromFile("textures/cerberus_M.png"),
            DXGI_FORMAT_R8_UNORM);
        m_RoughnessTexture = CreateTexture(Image::FromFile("textures/cerberus_R.png", 1), DXGI_FORMAT_R8_UNORM);
    }

    //创建SkyBox的根签名以及PSO
    {
        const std::vector<D3D12_INPUT_ELEMENT_DESC> SkyboxInputLayout = {
            { "POSITION",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT,
                0,
                0,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0 },
        };

        ComPtr<ID3DBlob> SkyboxVS = compileShader(
            "shaders/hlsl/skybox.hlsl", 
            "main_vs", 
            "vs_5_0");

        ComPtr<ID3DBlob> SkyboxPS = compileShader(
            "shaders/hlsl/skybox.hlsl", 
            "main_ps", 
            "ps_5_0");

        const CD3DX12_DESCRIPTOR_RANGE1 descriptorRanges[] = {
            {D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC},
            {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC},
        };
        CD3DX12_ROOT_PARAMETER1 rootParameters[2];
        rootParameters[0].InitAsDescriptorTable(1, &descriptorRanges[0], D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[1].InitAsDescriptorTable(1, &descriptorRanges[1], D3D12_SHADER_VISIBILITY_PIXEL);
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC signatureDesc = {};
        signatureDesc.Init_1_1(2, rootParameters, 1, &DefaultSamplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        m_SkyBoxRootSignature = CreateRootSignature(signatureDesc);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = m_SkyBoxRootSignature.Get();
        psoDesc.InputLayout = { SkyboxInputLayout.data(), (UINT)SkyboxInputLayout.size() };
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(SkyboxVS.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(SkyboxPS.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.FrontCounterClockwise = true;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        psoDesc.SampleDesc.Count = m_FrameBuffers[0].Samples;
        psoDesc.SampleMask = UINT_MAX;

        if (FAILED(m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_SkyBoxPipelineState)))) {
            throw std::runtime_error("Failed to create graphics pipeline state for skybox");
        }
    }

    //加载天空盒
    m_SkyBox = CreateMeshBuffer(Mesh::FromFile("meshes/skybox.obj"));

    //加载并且预先计算环境
    {
        ComPtr<ID3D12RootSignature> ComputeRootSignature;

        ID3D12DescriptorHeap* ComputeDescriptorHeaps[] = {
            m_DescHeapCBV_SRV_UAV.Heap.Get()
        };

        //创建通用计算根签名
        {
            const CD3DX12_DESCRIPTOR_RANGE1 descriptorRanges[] = 
            {
                {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC},
                {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE},
            };
            CD3DX12_ROOT_PARAMETER1 rootParameters[3];
            rootParameters[0].InitAsDescriptorTable(1, &descriptorRanges[0]);
            rootParameters[1].InitAsDescriptorTable(1, &descriptorRanges[1]);
            rootParameters[2].InitAsConstants(1, 0);
            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC signatureDesc;
            signatureDesc.Init_1_1(3, rootParameters, 1, &ComputeSamplerDesc);
            ComputeRootSignature = CreateRootSignature(signatureDesc);
        }

        m_EnvTexture = CreateTexture(1024, 1024, 6, DXGI_FORMAT_R16G16B16A16_FLOAT);
        {
            //Todo mark
            DescriptorHeapMark mark(m_DescHeapCBV_SRV_UAV);

            Texture envTextureUnfiltered = CreateTexture(1024, 1024, 6, DXGI_FORMAT_R16G16B16A16_FLOAT);
            CreateTextureUAV(envTextureUnfiltered, 0);

            //转换Equirectangular为Cubemap
            {
                DescriptorHeapMark mark(m_DescHeapCBV_SRV_UAV);

                Texture envTextureEquirect = CreateTexture(
                    Image::FromFile("environment1.hdr"),
                    DXGI_FORMAT_R32G32B32A32_FLOAT,
                    1);

                ComPtr<ID3D12PipelineState> pipelineState;
                ComPtr<ID3DBlob> equirectToCubemapShader = compileShader(
                    "shaders/hlsl/equirect2cube.hlsl",
                    "main",
                    "cs_5_0");

                D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
                psoDesc.pRootSignature = ComputeRootSignature.Get();
                psoDesc.CS = CD3DX12_SHADER_BYTECODE(equirectToCubemapShader.Get());
                if (FAILED(m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState))))
                {
                    throw std::runtime_error("Failed to create compute pipeline state (equirect2cube)");
                }

                auto Common2UA = CD3DX12_RESOURCE_BARRIER::Transition(envTextureUnfiltered.texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

                auto UA2Common = CD3DX12_RESOURCE_BARRIER::Transition(envTextureUnfiltered.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
                m_CommandList->ResourceBarrier(1, &Common2UA);
                m_CommandList->SetDescriptorHeaps(1, ComputeDescriptorHeaps);
                m_CommandList->SetPipelineState(pipelineState.Get());
                m_CommandList->SetComputeRootSignature(ComputeRootSignature.Get());
                m_CommandList->SetComputeRootDescriptorTable(0, envTextureEquirect.Srv.GpuHandle);
                m_CommandList->SetComputeRootDescriptorTable(1, envTextureUnfiltered.Uav.GpuHandle);
                m_CommandList->Dispatch(m_EnvTexture.Width / 32, m_EnvTexture.Height / 32, 6);
                m_CommandList->ResourceBarrier(1, &UA2Common);

                GenerateMipmaps(envTextureUnfiltered);
            }

            //计算Pre-filtered Specular
            {
                DescriptorHeapMark mark(m_DescHeapCBV_SRV_UAV);

                ComPtr<ID3D12PipelineState> PipelineStae;
                ComPtr<ID3DBlob> spmapShader = compileShader(
                    "shaders/hlsl/spmap.hlsl",
                    "main",
                    "cs_5_0");

                D3D12_COMPUTE_PIPELINE_STATE_DESC PsoDesc = {};
                PsoDesc.pRootSignature = ComputeRootSignature.Get();
                PsoDesc.CS = CD3DX12_SHADER_BYTECODE{ spmapShader.Get() };
                if (FAILED(m_Device->CreateComputePipelineState(&PsoDesc, IID_PPV_ARGS(&PipelineStae))))
                {
                    throw std::runtime_error("Failed to create compute pipeline state (spmap)");
                }

                // Copy 0th mipmap level into destination environment map.
                const D3D12_RESOURCE_BARRIER preCopyBarriers[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_EnvTexture.texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST),
                    CD3DX12_RESOURCE_BARRIER::Transition(envTextureUnfiltered.texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE)
                };

                const D3D12_RESOURCE_BARRIER postCopyBarriers[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(m_EnvTexture.texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                    CD3DX12_RESOURCE_BARRIER::Transition(envTextureUnfiltered.texture.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                };

                m_CommandList->ResourceBarrier(2, preCopyBarriers);
                for (UINT arraySlice = 0; arraySlice < 6; ++arraySlice) 
                {
                    const UINT subresourceIndex = D3D12CalcSubresource(0, arraySlice, 0, m_EnvTexture.Levels, 6);

                    auto Copy1 = CD3DX12_TEXTURE_COPY_LOCATION{ m_EnvTexture.texture.Get(), subresourceIndex };

                    auto Copy2 = CD3DX12_TEXTURE_COPY_LOCATION{ envTextureUnfiltered.texture.Get(), subresourceIndex };

                    m_CommandList->CopyTextureRegion(&Copy1, 0, 0, 0, &Copy2, nullptr);
                }
                m_CommandList->ResourceBarrier(2, postCopyBarriers);

                // Pre-filter rest of the mip chain.
                m_CommandList->SetDescriptorHeaps(1, ComputeDescriptorHeaps);
                m_CommandList->SetPipelineState(PipelineStae.Get());
                m_CommandList->SetComputeRootSignature(ComputeRootSignature.Get());
                m_CommandList->SetComputeRootDescriptorTable(0, envTextureUnfiltered.Srv.GpuHandle);

                const float deltaRoughness = 1.0f / std::max(float(m_EnvTexture.Levels - 1), 1.0f);
                for (UINT level = 1, size = 512; level < m_EnvTexture.Levels; ++level, size /= 2) {
                    const UINT numGroups = std::max<UINT>(1, size / 32);
                    const float spmapRoughness = level * deltaRoughness;

                    CreateTextureUAV(m_EnvTexture, level);

                    m_CommandList->SetComputeRootDescriptorTable(1, m_EnvTexture.Uav.GpuHandle);
                    m_CommandList->SetComputeRoot32BitConstants(2, 1, &spmapRoughness, 0);
                    m_CommandList->Dispatch(numGroups, numGroups, 6);
                }

                auto trans = CD3DX12_RESOURCE_BARRIER::Transition(m_EnvTexture.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

                m_CommandList->ResourceBarrier(1, &trans);

                ExecuteCommandList();
                WaitForGPU();
            }
        }

        //计算Irradiance cubemap
        m_IrMapTexture = CreateTexture(32, 32, 6, DXGI_FORMAT_R16G16B16A16_FLOAT, 1);
        {
            DescriptorHeapMark mark(m_DescHeapCBV_SRV_UAV);
            CreateTextureUAV(m_IrMapTexture, 0);

            ComPtr<ID3D12PipelineState> PipelineState;
            ComPtr<ID3DBlob> IrmapShader = compileShader(
                "Shaders/hlsl/irmap.hlsl",
                "main",
                "cs_5_0"
            );

            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = ComputeRootSignature.Get();
            psoDesc.CS = CD3DX12_SHADER_BYTECODE{ IrmapShader.Get() };
            if (FAILED(m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&PipelineState)))) 
            {
                throw std::runtime_error("Failed to create compute pipeline state (irmap)");
            }

            auto Common2UA = CD3DX12_RESOURCE_BARRIER::Transition(m_IrMapTexture.texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            auto UA2Common = CD3DX12_RESOURCE_BARRIER::Transition(m_IrMapTexture.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

            m_CommandList->ResourceBarrier(1, &Common2UA);
            m_CommandList->SetDescriptorHeaps(1, ComputeDescriptorHeaps);
            m_CommandList->SetPipelineState(PipelineState.Get());
            m_CommandList->SetComputeRootSignature(ComputeRootSignature.Get());
            m_CommandList->SetComputeRootDescriptorTable(0, m_EnvTexture.Srv.GpuHandle);
            m_CommandList->SetComputeRootDescriptorTable(1, m_IrMapTexture.Uav.GpuHandle);
            m_CommandList->Dispatch(m_IrMapTexture.Width / 32, m_IrMapTexture.Height / 32, 6);
            m_CommandList->ResourceBarrier(1, &UA2Common);

            ExecuteCommandList();
            WaitForGPU();

        }

        // 计算 Cook-Torrance BRDF 2D LUT for split-sum approximation.
        m_spBRDF_LUT = CreateTexture(256, 256, 1, DXGI_FORMAT_R16G16_FLOAT, 1);
        {
            DescriptorHeapMark mark(m_DescHeapCBV_SRV_UAV);
            CreateTextureUAV(m_spBRDF_LUT, 0);

            ComPtr<ID3D12PipelineState> pipelineState;
            ComPtr<ID3DBlob> spBRDFShader = compileShader("shaders/hlsl/spbrdf.hlsl", "main", "cs_5_0");

            D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = ComputeRootSignature.Get();
            psoDesc.CS = CD3DX12_SHADER_BYTECODE{ spBRDFShader.Get() };
            if (FAILED(m_Device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)))) {
                throw std::runtime_error("Failed to create compute pipeline state (spbrdf)");
            }

            auto Common2UA = CD3DX12_RESOURCE_BARRIER::Transition(m_spBRDF_LUT.texture.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            auto UA2Common = CD3DX12_RESOURCE_BARRIER::Transition(m_spBRDF_LUT.texture.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);

            m_CommandList->ResourceBarrier(1, &Common2UA);
            m_CommandList->SetDescriptorHeaps(1, ComputeDescriptorHeaps);
            m_CommandList->SetPipelineState(pipelineState.Get());
            m_CommandList->SetComputeRootSignature(ComputeRootSignature.Get());
            m_CommandList->SetComputeRootDescriptorTable(1, m_spBRDF_LUT.Uav.GpuHandle);
            m_CommandList->Dispatch(m_spBRDF_LUT.Width / 32, m_spBRDF_LUT.Height / 32, 1);
            m_CommandList->ResourceBarrier(1, &UA2Common);

            ExecuteCommandList();
            WaitForGPU();
        }
    }

    //创建64KB host-mappedBuffer在Uploadheap上来给shaderconstants
    m_constantBuffer = CreateUploadBuffer(64 * 1024); // 64 KB

    //配置每帧的Resource
    {
        std::vector<D3D12_RESOURCE_BARRIER> barriers{ NumFrames };
        for(UINT FrameIndex = 0 ; FrameIndex < NumFrames ; ++FrameIndex)
        {
            m_TransformCBVs[FrameIndex] = CreateConstantBufferView<TransformCB>();
            m_ShadingCBVs[FrameIndex] = CreateConstantBufferView<ShadingCB>();

            barriers[FrameIndex] = CD3DX12_RESOURCE_BARRIER::Transition(m_ResolveFrameBuffers[FrameIndex].ColorTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }

        m_CommandList->ResourceBarrier(NumFrames, barriers.data());
    }

    ExecuteCommandList(false);
    WaitForGPU();
}

void D3D12Renderer::Render(GLFWwindow* Window, const ViewSettings& view, const SceneSettings& Scene)
{
    Mat4 ProjectionMatrix = glm::perspectiveFov(view.fov, float(1024), float(1024), 1.0f, 1000.0f);
    const Mat4 ViewRotationMatrix = glm::eulerAngleXY(glm::radians(view.pitch), glm::radians(view.yaw));
    const Mat4 SceneRotationMatrix = glm::eulerAngleXY(glm::radians(Scene.pitch), glm::radians(Scene.yaw));
    const Mat4 ViewMatrix = glm::translate(Mat4{ 1.0f }, { 0.0f,0.0f,-view.distance }) * ViewRotationMatrix;

    const Vec3 EyePosition = glm::inverse(ViewMatrix)[3];
    const ConstantBufferView& transformCBV = m_TransformCBVs[m_FrameIndex];
    const ConstantBufferView& shadingCBV = m_ShadingCBVs[m_FrameIndex];

    //更新Transform constant
    {
        TransformCB* transformConstants = transformCBV.as<TransformCB>();
        transformConstants->ViewPorjectionMatrix = ProjectionMatrix * ViewMatrix;
        transformConstants->SkyProjectionMatrix = ProjectionMatrix * ViewRotationMatrix;
        transformConstants->SceneRotationMatix = SceneRotationMatrix;
    }

    //更新Shading constant
    {
        ShadingCB* ShadingConstants = shadingCBV.as<ShadingCB>();
        ShadingConstants->EyePosition = Vec4{ EyePosition,0.0f };
        for(int i =0 ; i < SceneSettings::NumLights ; ++i)
        {
            const SceneSettings::Light& light = Scene.Lights[i];
            ShadingConstants->Light[i].Direction = Vec4{ light.Direction,0.0f };
            if(light.Enabel)
            {
                ShadingConstants->Light[i].Radiance = Vec4{ light.Radiance,0.0f };
            }
            else
            {
                ShadingConstants->Light[i].Radiance = Vec4{1.0,1.0,1.0,0.0 };
            }
        }
    }

    ID3D12CommandAllocator* commandAllocator = m_CommandAllocators[m_FrameIndex].Get();
    const SwapChainBuffer& backbuffer = m_BackBuffers[m_FrameIndex];
    const FrameBuffer& framebuffer = m_FrameBuffers[m_FrameIndex];
    const FrameBuffer& resolveFramebuffer = m_ResolveFrameBuffers[m_FrameIndex];

    commandAllocator->Reset();
    m_CommandList->Reset(commandAllocator, m_SkyBoxPipelineState.Get());

    //设置全局状态

    auto Viewport = CD3DX12_VIEWPORT{ 0.0f, 0.0f, (FLOAT)framebuffer.Width, (FLOAT)framebuffer.Height };

    auto ScissRect = CD3DX12_RECT{ 0, 0, (LONG)framebuffer.Width, (LONG)framebuffer.Height };

    m_CommandList->RSSetViewports(1, &Viewport);
    m_CommandList->RSSetScissorRects(1, &ScissRect);

    ID3D12DescriptorHeap* DescriptorHeap[] =
    {
        m_DescHeapCBV_SRV_UAV.Heap.Get()
    };

    m_CommandList->SetDescriptorHeaps(1, DescriptorHeap);

    if(framebuffer.Samples <= 1)
    {
        auto ResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(framebuffer.ColorTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_CommandList->ResourceBarrier(1, &ResourceBarrier);
    }

    //准备渲染到Main FrameBuffer

    m_CommandList->OMSetRenderTargets(1, &framebuffer.Rtv.CpuHandle, false, &framebuffer.Dsv.CpuHandle);

    m_CommandList->ClearDepthStencilView(framebuffer.Dsv.CpuHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //绘制Skybox
    {
        m_CommandList->SetGraphicsRootSignature(m_SkyBoxRootSignature.Get());
        m_CommandList->SetGraphicsRootDescriptorTable(0, transformCBV.Cbv.GpuHandle);
        m_CommandList->SetGraphicsRootDescriptorTable(1, m_EnvTexture.Srv.GpuHandle);
             
        m_CommandList->IASetVertexBuffers(0, 1, &m_SkyBox.Vbv);
        m_CommandList->IASetIndexBuffer(&m_SkyBox.Ibv);
             
        m_CommandList->DrawIndexedInstanced(m_SkyBox.NumElements, 1, 0, 0, 0);
    }

    //绘制 PBR model
    {
        m_CommandList->SetGraphicsRootSignature(m_PbrRootSignature.Get());
        m_CommandList->SetGraphicsRootDescriptorTable(0, transformCBV.Cbv.GpuHandle);
        m_CommandList->SetGraphicsRootDescriptorTable(1, shadingCBV.Cbv.GpuHandle);
        m_CommandList->SetGraphicsRootDescriptorTable(2, m_AlbedoTexture.Srv.GpuHandle);
        m_CommandList->SetPipelineState(m_PbrPipelineState.Get());

        m_CommandList->IASetVertexBuffers(0, 1, &m_PbrModel.Vbv);
        m_CommandList->IASetIndexBuffer(&m_PbrModel.Ibv);

        m_CommandList->DrawIndexedInstanced(m_PbrModel.NumElements, 1, 0, 0, 0);
    }

    if(framebuffer.Samples > 1)
    {
        ResolveFrameBuffer(framebuffer, resolveFramebuffer, DXGI_FORMAT_R16G16B16A16_FLOAT);
    }
    else
    {
        auto Rt2ShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(framebuffer.ColorTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_CommandList->ResourceBarrier(1, &Rt2ShaderResource);
    }

    //  准备渲染到BackBuffer中
    auto Present2RT = CD3DX12_RESOURCE_BARRIER::Transition(backbuffer.Buffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_CommandList->ResourceBarrier(1, &Present2RT);
    m_CommandList->OMSetRenderTargets(1, &backbuffer.Rtv.CpuHandle, false, nullptr);

    //画一个全屏三角形来做后处理
    {
        m_CommandList->SetGraphicsRootSignature(m_ToneMapRootSignature.Get());
        m_CommandList->SetGraphicsRootDescriptorTable(0, resolveFramebuffer.Srv.GpuHandle);
        m_CommandList->SetPipelineState(m_ToneMapPipelineState.Get());

        m_CommandList->DrawInstanced(3, 1, 0, 0);
    }

    auto RT2PRE = CD3DX12_RESOURCE_BARRIER::Transition(backbuffer.Buffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_CommandList->ResourceBarrier(1, &RT2PRE);

    ExecuteCommandList(false);
    PresentFrame();
}

DescriptorHeap D3D12Renderer::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC& Desc) const
{
    DescriptorHeap Heap;
    if(FAILED(m_Device->CreateDescriptorHeap(&Desc,IID_PPV_ARGS(&Heap.Heap))))
    {
        throw std::runtime_error("Falied to Create Descriptor Heap");
    }

    Heap.NumDescriptorsAllocated = 0;
    Heap.NumDescriptorsInHeap = Desc.NumDescriptors;
    Heap.DescriptorSize = m_Device->GetDescriptorHandleIncrementSize(Desc.Type);

    return Heap;
}

MeshBuffer D3D12Renderer::CreateMeshBuffer(const std::shared_ptr<Mesh> mesh) const
{
    MeshBuffer Buffer;
    Buffer.NumElements = static_cast<UINT>(mesh->Faces().size() * 3);

    const size_t VertexDataSize = mesh->Vertices().size() * sizeof(Mesh::Vertex);
    const size_t IndexDataSize = mesh->Faces().size() * sizeof(Mesh::Face);

    //创建GPU端资源
    auto DefaultType = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
    auto ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(VertexDataSize);
    if (FAILED(m_Device->CreateCommittedResource(
        &DefaultType,
        D3D12_HEAP_FLAG_NONE,
        &ResourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&Buffer.VertexBuffer)
    )))
    {
        throw std::runtime_error("Failed to create vertex buffer");
    }

    Buffer.Vbv.BufferLocation = Buffer.VertexBuffer->GetGPUVirtualAddress();
    Buffer.Vbv.SizeInBytes = static_cast<UINT>(VertexDataSize);
    Buffer.Vbv.StrideInBytes = sizeof(Mesh::Vertex);

    auto IndexDesc = CD3DX12_RESOURCE_DESC::Buffer(IndexDataSize);
    if (FAILED(m_Device->CreateCommittedResource(
        &DefaultType,
        D3D12_HEAP_FLAG_NONE,
        &IndexDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&Buffer.IndexBuffer)
    )))
    {
        throw std::runtime_error("Failed to create index buffer");
    }
    Buffer.Ibv.BufferLocation = Buffer.IndexBuffer->GetGPUVirtualAddress();
    Buffer.Ibv.SizeInBytes = static_cast<UINT>(IndexDataSize);
    Buffer.Ibv.Format = DXGI_FORMAT_R32_UINT;

    //创建一个临时缓冲区
    StagingBuffer VertexStagingBuffer;
    {
        const D3D12_SUBRESOURCE_DATA Data = { mesh->Vertices().data() };
        VertexStagingBuffer = CreateStagingBuffer(Buffer.VertexBuffer, 0, 1, &Data);
    }
    StagingBuffer IndexStagingBuffer;
    {
        const D3D12_SUBRESOURCE_DATA Data = { mesh->Faces().data() };
        IndexStagingBuffer = CreateStagingBuffer(Buffer.IndexBuffer, 0, 1, &Data);
    }

    //上传到GPU
    m_CommandList->CopyResource(Buffer.VertexBuffer.Get(), VertexStagingBuffer.Buffer.Get());
    m_CommandList->CopyResource(Buffer.IndexBuffer.Get(), IndexStagingBuffer.Buffer.Get());

    const D3D12_RESOURCE_BARRIER Barriers[] =
    {
        CD3DX12_RESOURCE_BARRIER::Transition(
        Buffer.VertexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),

        CD3DX12_RESOURCE_BARRIER::Transition(
            Buffer.IndexBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_INDEX_BUFFER
        )
    };

    m_CommandList->ResourceBarrier(2, Barriers);
    ExecuteCommandList();
    WaitForGPU();

    return Buffer;
}

UploadBuffer D3D12Renderer::CreateUploadBuffer(UINT Capacity) const
{
    UploadBuffer buffer;
    buffer.Cursor = 0;
    buffer.Capacity = Capacity;

    auto HeapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto ResourceDesc = CD3DX12_RESOURCE_DESC::Buffer(Capacity);

    if (FAILED(m_Device->CreateCommittedResource(
        &HeapProperty,
        D3D12_HEAP_FLAG_NONE,
        &ResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer.Buffer))))
    {
        throw std::runtime_error("Failed to create GPU upload buffer");
    }

    auto Range = CD3DX12_RANGE{ 0, 0 };
    if (FAILED(buffer.Buffer->Map(0, &Range, reinterpret_cast<void**>(&buffer.CpuAddress)))) 
    {
        throw std::runtime_error("Failed to map GPU upload buffer to host address space");
    }
    buffer.GpuAddress = buffer.Buffer->GetGPUVirtualAddress();
    return buffer;
}

UploadBufferRegion D3D12Renderer::AllocFromUploadBuffer(UploadBuffer& Buffer, UINT Size, int Align) const
{
    const UINT AlignedSize = Utils::roundToPowerOfTwo(Size, Align);
    if(Buffer.Cursor + AlignedSize > Buffer.Capacity)
    {
        throw std::overflow_error("Out of upload buffer capacity while allocating memory");
    }

    UploadBufferRegion Region;
    Region.CpuAddress = reinterpret_cast<void*>(Buffer.CpuAddress + Buffer.Cursor);
    Region.GpuAddress = Buffer.GpuAddress + Buffer.Cursor;
    Region.Size = AlignedSize;
    Buffer.Cursor += AlignedSize;

    return Region;
}

StagingBuffer D3D12Renderer::CreateStagingBuffer(const ComPtr<ID3D12Resource>& Resource, UINT FirstSubresource,UINT NumSubResources, const D3D12_SUBRESOURCE_DATA* Data) const
{
    StagingBuffer stagingBuffer;
    stagingBuffer.FirstSubResource = FirstSubresource;
    stagingBuffer.NumSubResource = NumSubResources;
    stagingBuffer.Layouts.resize(NumSubResources);

    const D3D12_RESOURCE_DESC ResourceDesc = Resource->GetDesc();

    UINT64 NumBytesTotal;
    std::vector<UINT> numRows{ NumSubResources };
    std::vector<UINT64> rowBytes{ NumSubResources };
    m_Device->GetCopyableFootprints(
        &ResourceDesc,
        FirstSubresource,
        NumSubResources,
        0,
        stagingBuffer.Layouts.data(),
        numRows.data(),
        rowBytes.data(),
        &NumBytesTotal);


    auto UploadType = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(NumBytesTotal);
    if (FAILED(m_Device->CreateCommittedResource(
        &UploadType,
        D3D12_HEAP_FLAG_NONE,
        &BufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&stagingBuffer.Buffer)
    )))
    {
        throw std::runtime_error("Failed to create GPU staging buffer");
    }

    if(Data)
    {
        assert(ResourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D);

        auto Range = CD3DX12_RANGE{ 0,0 };
        void* BufferMemory;
        if(FAILED(stagingBuffer.Buffer->Map(0,&Range,&BufferMemory)))
        {
            throw std::runtime_error("Failed to map GPU staging buffer to host address space");
        }

        for(UINT subresource = 0;subresource < NumSubResources;++subresource)
        {
            uint8_t* subresourceMemory = reinterpret_cast<uint8_t*>(BufferMemory) + stagingBuffer.Layouts[subresource].Offset;

            //线性的话就一把梭
            if(ResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
            {
                std::memcpy(subresourceMemory, Data->pData, NumBytesTotal);
            }
            else
            {
                //Texture就需要逐行复制
                for (UINT row = 0; row < numRows[subresource]; ++row) 
                {
                    const uint8_t* srcRowPtr = reinterpret_cast<const uint8_t*>(Data[subresource].pData) + row * Data[subresource].RowPitch;

                    uint8_t* destRowPtr = subresourceMemory + row * stagingBuffer.Layouts[subresource].Footprint.RowPitch;

                    std::memcpy(destRowPtr, srcRowPtr, rowBytes[subresource]);
                }
            }
        }

        stagingBuffer.Buffer->Unmap(0, nullptr);
    }

    return stagingBuffer;
}

FrameBuffer D3D12Renderer::CreateFrameBuffer(UINT Width, UINT Height, UINT Samples, DXGI_FORMAT ColorFormat,DXGI_FORMAT DepthStencilFormat)
{
    FrameBuffer fb = {  };
    fb.Width = Width;
    fb.Height = Height;
    fb.Samples = Samples;

    D3D12_RESOURCE_DESC Desc = {};
    Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.DepthOrArraySize = 1;
    Desc.MipLevels = 1;
    Desc.SampleDesc.Count = Samples;

    if (ColorFormat != DXGI_FORMAT_UNKNOWN)
    {
        Desc.Format = ColorFormat;
        Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        const float OptimizedClearColor[] = { 0.0f,0.0f,0.0f,0.0f };

        auto HeapProPerties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
        auto ClearColor = CD3DX12_CLEAR_VALUE{ ColorFormat,OptimizedClearColor };
        if (FAILED(m_Device->CreateCommittedResource(
            &HeapProPerties,
            D3D12_HEAP_FLAG_NONE,
            &Desc, D3D12_RESOURCE_STATE_RENDER_TARGET,
            &ClearColor,
            IID_PPV_ARGS(&fb.ColorTexture)
        )))
        {
            throw std::runtime_error("Failed to create FrameBuffer color texture");
        }

        D3D12_RENDER_TARGET_VIEW_DESC RtvDesc = {};
        RtvDesc.Format = Desc.Format;
        RtvDesc.ViewDimension = (Samples > 1) ?
            D3D12_RTV_DIMENSION_TEXTURE2DMS :
            D3D12_RTV_DIMENSION_TEXTURE2D;

        fb.Rtv = m_DescHeapRtv.Alloc();
        m_Device->CreateRenderTargetView(fb.ColorTexture.Get(), &RtvDesc, fb.Rtv.CpuHandle);

        if (Samples <= 1)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
            SrvDesc.Format = Desc.Format;
            SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            SrvDesc.Texture2D.MostDetailedMip = 0;
            SrvDesc.Texture2D.MipLevels = 1;

            fb.Srv = m_DescHeapCBV_SRV_UAV.Alloc();
            m_Device->CreateShaderResourceView(fb.ColorTexture.Get(), &SrvDesc, fb.Srv.CpuHandle);
        }
    }
    if(DepthStencilFormat != DXGI_FORMAT_UNKNOWN)
    {
        Desc.Format = DepthStencilFormat;
        Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |
            D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

        auto DeFaultHeap = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT);
        auto ClearDepth = CD3DX12_CLEAR_VALUE{ DepthStencilFormat,1.0f,0 };
        if (FAILED(m_Device->CreateCommittedResource(
            &DeFaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &Desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &ClearDepth,
            IID_PPV_ARGS(&fb.DepthStencilTexture)
        )))
        {
            throw std::runtime_error("Failed to create FrameBuffer depth-stencil texture");
        }

        D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
        DsvDesc.Format = Desc.Format;
        DsvDesc.ViewDimension = (Samples > 1) ?
            D3D12_DSV_DIMENSION_TEXTURE2DMS :
            D3D12_DSV_DIMENSION_TEXTURE2D;

        fb.Dsv = m_DescHeapDsv.Alloc();
        m_Device->CreateDepthStencilView(
            fb.DepthStencilTexture.Get(),
            &DsvDesc, 
            fb.Dsv.CpuHandle);
    }

    return fb;
}

void D3D12Renderer::ResolveFrameBuffer(const FrameBuffer& SourceBuffer, const FrameBuffer& DestBuffer,DXGI_FORMAT Format) const
{
    const CD3DX12_RESOURCE_BARRIER PreResolveBarriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(SourceBuffer.ColorTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(DestBuffer.ColorTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST)
    };

    const CD3DX12_RESOURCE_BARRIER PostResolveBarriers[] =
    {
    CD3DX12_RESOURCE_BARRIER::Transition(SourceBuffer.ColorTexture.Get(), D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET),
    CD3DX12_RESOURCE_BARRIER::Transition(DestBuffer.ColorTexture.Get(), D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    };

    if (SourceBuffer.ColorTexture != DestBuffer.ColorTexture)
    {
        m_CommandList->ResourceBarrier(2, PreResolveBarriers);
        m_CommandList->ResolveSubresource(DestBuffer.ColorTexture.Get(), 0, SourceBuffer.ColorTexture.Get(), 0, Format);
        m_CommandList->ResourceBarrier(2, PostResolveBarriers);
    }

}

ComPtr<ID3D12RootSignature> D3D12Renderer::CreateRootSignature(D3D12_VERSIONED_ROOT_SIGNATURE_DESC& Desc) const
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

    ComPtr<ID3D12RootSignature> RootSignature;
    ComPtr<ID3DBlob> SignatureBlob, ErrorBlob;

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

ConstantBufferView D3D12Renderer::CreateConstantBufferView(const void* Data, UINT size)
{
    ConstantBufferView View;
    View.Data = AllocFromUploadBuffer(m_constantBuffer, size, 256);
    View.Cbv = m_DescHeapCBV_SRV_UAV.Alloc();
    if(Data)
    {
        std::memcpy(View.Data.CpuAddress, Data, size);
    }

    D3D12_CONSTANT_BUFFER_VIEW_DESC Desc = {};
    Desc.BufferLocation = View.Data.GpuAddress;
    Desc.SizeInBytes = View.Data.Size;
    m_Device->CreateConstantBufferView(&Desc, View.Cbv.CpuHandle);

    return View;
}

Texture D3D12Renderer::CreateTexture(UINT Width, UINT Height, UINT Depth, DXGI_FORMAT Format, UINT Levels)
{
    assert(Depth == 1 || Depth == 6);

    Texture texture;
    texture.Width = Width;
    texture.Height = Height;
    texture.Levels = (Levels > 0) ? Levels:Utils::numMipmapLevels(Width, Height);

    D3D12_RESOURCE_DESC Desc = {};
    Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.DepthOrArraySize = Depth;
    Desc.MipLevels = Levels;
    Desc.Format = Format;
    Desc.SampleDesc.Count = 1;
    Desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

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
    CreateTextureSRV(texture, SrvDim);

    return texture;
}

Texture D3D12Renderer::CreateTexture(const std::shared_ptr<Image>& Image, DXGI_FORMAT Format, UINT Levels)
{
    Texture texture = CreateTexture(Image->Width(), Image->Height(), 1, Format, Levels);
    StagingBuffer TextureStagingBuffer;
    {
        const D3D12_SUBRESOURCE_DATA Data{ Image->Pixels<void>(),Image->Pitch() };
        TextureStagingBuffer = CreateStagingBuffer(texture.texture, 0, 1, &Data);
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

    if(texture.Levels > 1 && texture.Width == texture.Height && Utils::IsPowerOfTwo(texture.Width))
    {
        GenerateMipmaps(texture);
    }
    else {
        // Only execute & wait if not generating mipmaps (so that we don't wait redundantly).
        ExecuteCommandList();
        WaitForGPU();
    }

    return texture;
}

void D3D12Renderer::GenerateMipmaps(const Texture& texture)
{
    assert(texture.Width == texture.Height);
    assert(Utils::IsPowerOfTwo(texture.Width));

    if(!m_mipmapGeneration.RootSignature)
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
        m_mipmapGeneration.RootSignature = CreateRootSignature(rootSignatureDesc);
    }

    ID3D12PipelineState* pipelineState = nullptr;

    const D3D12_RESOURCE_DESC desc = texture.texture->GetDesc();
    if (desc.DepthOrArraySize == 1 && desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
        if (!m_mipmapGeneration.gammaTexturePipelineState) 
        {
            ComPtr<ID3DBlob> computeShader = compileShader(
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
            ComPtr<ID3DBlob> computeShader = compileShader(
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
            ComPtr<ID3DBlob> computeShader = compileShader(
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
            linearTexture, 
            desc.DepthOrArraySize > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DARRAY : D3D12_SRV_DIMENSION_TEXTURE2D, 
            level - 1, 
            1);

        CreateTextureUAV(linearTexture, level);

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

    ExecuteCommandList();
    WaitForGPU();
}

void D3D12Renderer::CreateTextureSRV(Texture& texture, D3D12_SRV_DIMENSION Dimension, UINT MostDetailedMip,UINT MipLevels)
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

void D3D12Renderer::CreateTextureUAV(Texture& texture, UINT mipSlice)
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

void D3D12Renderer::ExecuteCommandList(bool Reset) const
{
    if (FAILED(m_CommandList->Close())) 
    {
        throw std::runtime_error("Failed close command list (validation error or not in recording state)");
    }

    ID3D12CommandList* lists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(1, lists);

    if (Reset) 
    {
        m_CommandList->Reset(m_CommandAllocators[m_FrameIndex].Get(), nullptr);
    }
}

void D3D12Renderer::WaitForGPU() const
{
    UINT64& fenceValue = m_FenceValue[m_FrameIndex];
    ++fenceValue;

    m_CommandQueue->Signal(m_Fence.Get(), fenceValue);
    m_Fence->SetEventOnCompletion(fenceValue, m_FenceCompletionEvent);
    WaitForSingleObject(m_FenceCompletionEvent, INFINITE);
}

void D3D12Renderer::PresentFrame()
{
    m_SwapChain->Present(1, 0);
    const UINT64 prevFrameFenceValue = m_FenceValue[m_FrameIndex];
    m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
    UINT64& currentFrameFenceValue = m_FenceValue[m_FrameIndex];

    m_CommandQueue->Signal(m_Fence.Get(), prevFrameFenceValue);
    if (m_Fence->GetCompletedValue() < currentFrameFenceValue) 
    {
        m_Fence->SetEventOnCompletion(currentFrameFenceValue, m_FenceCompletionEvent);
        WaitForSingleObject(m_FenceCompletionEvent, INFINITE);
    }
    currentFrameFenceValue = prevFrameFenceValue + 1;
}

ComPtr<IDXGIAdapter1> D3D12Renderer::getAdapter(const ComPtr<IDXGIFactory4>& factory)
{
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT index = 0; factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
            return adapter;
        }
    }
    return nullptr;
}

ComPtr<ID3DBlob> D3D12Renderer::compileShader(const std::string& filename, const std::string& entryPoint,const std::string& profile)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if  _DEBUG
    flags |= D3DCOMPILE_DEBUG;
    flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> Shader;
    ComPtr<ID3DBlob> ErrorBlob;

    std::printf("Compiling HLSL shader: %s [%s]\n", filename.c_str(), entryPoint.c_str());

    if (FAILED(D3DCompileFromFile(
        Utils::convertToUTF16(filename).c_str(), 
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, 
        entryPoint.c_str(), 
        profile.c_str(), 
        flags, 
        0, 
        &Shader, 
        &ErrorBlob))) 
    {
        std::string ErrorMsg = "Shader compilation failed: " + filename;
        if (ErrorBlob) 
        {
            ErrorMsg += std::string("\n") + static_cast<const char*>(ErrorBlob->GetBufferPointer());
        }

        throw std::runtime_error(ErrorMsg);
    }
    return Shader;
}



