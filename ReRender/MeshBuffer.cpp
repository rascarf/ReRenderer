#include "MeshBuffer.h"
#include <stdexcept>
#include <d3dx12/d3dx12.h>
#include <d3dcompiler.h>

#include "Mesh.h"
#include "StagingBuffer.h"

MeshBuffer MeshBuffer::CreateMeshBuffer(std::function<void()>CallBack, ComPtr<ID3D12GraphicsCommandList> m_CommandList, ComPtr<ID3D12Device>m_Device, std::shared_ptr<class Mesh>mesh)
{
    MeshBuffer Buffer;
    Buffer.NumElements = static_cast<UINT>(mesh->Faces().size() * 3);

    const size_t VertexDataSize = mesh->Vertices().size() * sizeof(Vertex);
    const size_t IndexDataSize = mesh->Faces().size() * sizeof(Face);

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
    Buffer.Vbv.StrideInBytes = sizeof(Vertex);

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
        VertexStagingBuffer = StagingBuffer::CreateStagingBuffer(m_Device, Buffer.VertexBuffer, 0, 1, &Data);
    }
    StagingBuffer IndexStagingBuffer;
    {
        const D3D12_SUBRESOURCE_DATA Data = { mesh->Faces().data() };
        IndexStagingBuffer = StagingBuffer::CreateStagingBuffer(m_Device, Buffer.IndexBuffer, 0, 1, &Data);
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

    CallBack();

    return Buffer;
}

MeshBuffer MeshBuffer::CreateMeshBuffer(std::function<void()> CallBack, ComPtr<ID3D12GraphicsCommandList> m_CommandList,
    ComPtr<ID3D12Device> m_Device, MeshData& meshData)
{
    auto GeneratorMesh = std::make_shared<Mesh>(meshData);

    MeshBuffer buffer = CreateMeshBuffer(CallBack, m_CommandList, m_Device, GeneratorMesh);

    return buffer;
}
