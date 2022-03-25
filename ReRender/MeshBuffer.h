#pragma once
#include <memory>
#include <functional>
#include <wrl/client.h>
#include <memory>
#include <d3d12.h>
#include <dxgi1_4.h>

using Microsoft::WRL::ComPtr;

class MeshBuffer
{
public:
    Microsoft::WRL::ComPtr<ID3D12Resource> VertexBuffer;
    ComPtr<ID3D12Resource> IndexBuffer;
    D3D12_VERTEX_BUFFER_VIEW Vbv;
    D3D12_INDEX_BUFFER_VIEW Ibv;
    UINT NumElements;

    static MeshBuffer CreateMeshBuffer(std::function<void()>CallBack,ComPtr<ID3D12GraphicsCommandList> m_CommandList,ComPtr<ID3D12Device>m_Device, std::shared_ptr<class Mesh>mesh);

    static MeshBuffer CreateMeshBuffer(std::function<void()> CallBack, ComPtr<ID3D12GraphicsCommandList> m_CommandList,
        ComPtr<ID3D12Device> m_Device, class MeshData& meshData);
};

