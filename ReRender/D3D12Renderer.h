#pragma once

#include <memory>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include "renderer.h"
#include "utils.h"

using Microsoft::WRL::ComPtr;

struct Descriptor
{
    D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle;
};

struct DescriptorHeap
{
    ComPtr<ID3D12DescriptorHeap> Heap;
    UINT DescriptorSize;
    UINT NumDescriptorsInHeap;
    UINT NumDescriptorsAllocated;

    Descriptor Alloc()
    {
        return (*this)[NumDescriptorsAllocated++];
    }

    Descriptor operator[](UINT Index) const
    {
        assert(Index < NumDescriptorsInHeap);
        return {
            D3D12_CPU_DESCRIPTOR_HANDLE{Heap->GetCPUDescriptorHandleForHeapStart().ptr + Index * DescriptorSize},
            D3D12_GPU_DESCRIPTOR_HANDLE{Heap->GetGPUDescriptorHandleForHeapStart().ptr + Index * DescriptorSize }
        };
    }
};

struct DescriptorHeapMark
{
    DescriptorHeapMark(DescriptorHeap& InHeap)
        :Heap(InHeap)
        ,Mark(Heap.NumDescriptorsAllocated)
    {}

    ~DescriptorHeapMark()
    {
        Heap.NumDescriptorsAllocated = Mark;
    }

    DescriptorHeap& Heap;
    const UINT Mark;
};

//todo change it!
struct StagingBuffer
{
    ComPtr<ID3D12Resource> Buffer;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> Layouts;
    UINT FirstSubResource;
    UINT NumSubResource;
};

struct SwapChainBuffer
{
    ComPtr<ID3D12Resource> Buffer;
    Descriptor Rtv;
};

struct UploadBuffer
{
    ComPtr<ID3D12Resource> Buffer;
    UINT Capacity;
    UINT Cursor;
    D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
    uint8_t* CpuAddress;
};

struct UploadBufferRegion
{
    void* CpuAddress;
    D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
    UINT Size;
};

struct MeshBuffer
{
    ComPtr<ID3D12Resource> VertexBuffer;
    ComPtr<ID3D12Resource> IndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW Vbv;
    D3D12_INDEX_BUFFER_VIEW Ibv;
    UINT NumElements;
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

struct ConstantBufferView
{
    UploadBufferRegion Data;
    Descriptor Cbv;

    template<typename T>T* as() const
    {
        return reinterpret_cast<T*>(Data.CpuAddress);
    }

};

struct Texture
{
    ComPtr<ID3D12Resource> texture;
    Descriptor Srv;
    Descriptor Uav;
    UINT Width, Height;
    UINT Levels;
};


class D3D12Renderer final : public RendererInterface
{
public:
    GLFWwindow* initialize(int Width, int Height, int MaxSamples) override;
    void ShutDown() override;
    void Setup() override;
    void Render(GLFWwindow* Window, const ViewSettings& view, const SceneSettings& Scene) override;


private:
    DescriptorHeap CreateDescriptorHeap(
        const D3D12_DESCRIPTOR_HEAP_DESC& Desc
    )const;

    MeshBuffer CreateMeshBuffer(
        const std::shared_ptr<class Mesh>mesh
    )const;

    UploadBuffer CreateUploadBuffer(UINT Capacity)const;

    UploadBufferRegion AllocFromUploadBuffer(
        UploadBuffer& Buffer,
        UINT Size,
        int Align
    )const;

    StagingBuffer CreateStagingBuffer(
        const ComPtr<ID3D12Resource>& Resource,
        UINT FirstSubresource, 
        UINT NumSubResources, 
        const D3D12_SUBRESOURCE_DATA* Data
    )const;

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

    ComPtr<ID3D12RootSignature> CreateRootSignature(
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC& Desc
    )const;

    ConstantBufferView CreateConstantBufferView(
        const void* Data,
        UINT size
    );
    template<typename T>
    ConstantBufferView CreateConstantBufferView
    (
        const T* Data = nullptr
    )
    {
        return CreateConstantBufferView(Data, sizeof(T));
    }

    //Texture
    Texture CreateTexture(
        UINT Width, 
        UINT Height, 
        UINT Depth, 
        DXGI_FORMAT Format, 
        UINT Levels = 0
    );
    Texture CreateTexture(
        const std::shared_ptr<class Image>& Image, DXGI_FORMAT Format, 
        UINT Levels = 0
    );

    void GenerateMipmaps(const Texture& texture);

    void CreateTextureSRV(
        Texture& texture, 
        D3D12_SRV_DIMENSION Dimension, 
        UINT MostDetailedMip = 0, 
        UINT MipLevels = 0
    );

    void CreateTextureUAV(
        Texture& texture, 
        UINT mipSlice
    );

    void ExecuteCommandList(bool Reset = true)const;
    void WaitForGPU()const;
    void PresentFrame();

    static ComPtr<IDXGIAdapter1> getAdapter(const ComPtr<IDXGIFactory4>& factory);
    static ComPtr<ID3DBlob> compileShader(const std::string& filename, const std::string& entryPoint, const std::string& profile);


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

    struct
    {
        ComPtr<ID3D12RootSignature> RootSignature;
        ComPtr<ID3D12PipelineState> LinearTexturePipelineState;
        ComPtr<ID3D12PipelineState> gammaTexturePipelineState;
        ComPtr<ID3D12PipelineState>ArrayTexturePipelineState;
    }m_mipmapGeneration;

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

    UINT m_FrameIndex;
    ComPtr<ID3D12Fence> m_Fence;
    HANDLE m_FenceCompletionEvent;
    mutable UINT64 m_FenceValue[NumFrames] = {};

    D3D_ROOT_SIGNATURE_VERSION m_RootSignatureVersion;

};

