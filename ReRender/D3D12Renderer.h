#pragma once

#include <memory>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include "Descriptor.h"
#include "MeshBuffer.h"
#include "renderer.h"
#include "ShadowMap.h"
#include "StagingBuffer.h"
#include "Texture.h"
#include "UploadBuffer.h"
#include "utils.h"


using Microsoft::WRL::ComPtr;

struct SwapChainBuffer
{
    ComPtr<ID3D12Resource> Buffer;
    Descriptor Rtv;
};


struct FrameBuffer
{
    ComPtr<ID3D12Resource> ColorTexture;
    ComPtr<ID3D12Resource> DepthStencilTexture;
    Descriptor Rtv;
    Descriptor Dsv;
    Descriptor Srv;
    UINT Width, Height;
    UINT Samples;
};


class D3D12Renderer final : public RendererInterface
{
public:
    GLFWwindow* initialize(int Width, int Height, int MaxSamples) override;
    void ShutDown() override;
    void Setup(const ViewSettings& view, const SceneSettings& Scene) override;
    void Update(const float DeltaTime) override;
    void Render(GLFWwindow* Window,const float DeltaTime) override;

    void SetLight()override;


private:
    FrameBuffer CreateFrameBuffer(
        UINT Width,
        UINT Height,
        UINT Samples,
        DXGI_FORMAT ColorFormat,
        DXGI_FORMAT DepthStencilFormat
    );

    void ResolveFrameBuffer(
        const FrameBuffer& SourceBuffer,
        const FrameBuffer& DestBuffer,
        DXGI_FORMAT Format
    )const;


    void ExecuteCommandList(bool Reset = true)const;
    void WaitForGPU()const;
    void PresentFrame();

    static ComPtr<IDXGIAdapter1> getAdapter(const ComPtr<IDXGIFactory4>& factory);


    ComPtr<ID3D12Device> m_Device;
    ComPtr<ID3D12CommandQueue> m_CommandQueue;
    ComPtr<IDXGISwapChain3> m_SwapChain;
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;

    DescriptorHeap m_DescHeapRtv;
    DescriptorHeap m_DescHeapDsv;
    DescriptorHeap m_DescHeapCBV_SRV_UAV;

    UploadBuffer m_constantBuffer;

    static const UINT NumFrames = 2;
    ComPtr<ID3D12CommandAllocator>m_CommandAllocators[NumFrames];
    SwapChainBuffer m_BackBuffers[NumFrames];

    FrameBuffer m_FrameBuffers[NumFrames];
    FrameBuffer m_ResolveFrameBuffers[NumFrames];

    ConstantBufferView m_TransformCBVs[NumFrames];
    ConstantBufferView m_ShadingCBVs[NumFrames];
    ConstantBufferView m_ShadowMapCBVs[NumFrames];

    MipMapGeneration m_mipmapGeneration;

    MeshBuffer m_PbrModel;
    MeshBuffer m_SkyBox;

    Texture m_AlbedoTexture;
    Texture m_NormalTexture;
    Texture m_MetalnessTexture;
    Texture m_RoughnessTexture;

    Texture m_EnvTexture;
    Texture m_IrMapTexture;
    Texture m_spBRDF_LUT;

    ComPtr<ID3D12RootSignature> m_ToneMapRootSignature;
    ComPtr<ID3D12PipelineState>m_ToneMapPipelineState;

    ComPtr<ID3D12RootSignature> m_PbrRootSignature;
    ComPtr<ID3D12PipelineState>m_PbrPipelineState;

    ComPtr<ID3D12RootSignature> m_SkyBoxRootSignature;
    ComPtr<ID3D12PipelineState> m_SkyBoxPipelineState;

    std::unique_ptr<ShadowMap> m_ShadowMap;

    UINT m_FrameIndex;
    ComPtr<ID3D12Fence> m_Fence;
    HANDLE m_FenceCompletionEvent;
    mutable UINT64 m_FenceValue[NumFrames] = {};

    D3D_ROOT_SIGNATURE_VERSION m_RootSignatureVersion;

    ViewSettings m_View;
    SceneSettings m_Scene;

};

