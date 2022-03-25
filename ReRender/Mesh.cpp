#include "Mesh.h"

#include <assimp/include/assimp/scene.h>
#include <assimp/include/assimp/Importer.hpp>
#include <assimp/include/assimp/postprocess.h>
#include <assimp/include/assimp/DefaultLogger.hpp>
#include <assimp/include/assimp/LogStream.hpp>

#include <cstdio>
#include <stdexcept>

const unsigned int ImportFlags =
    aiProcess_CalcTangentSpace |
    aiProcess_Triangulate |
    aiProcess_SortByPType |
    aiProcess_PreTransformVertices |
    aiProcess_GenNormals |
    aiProcess_GenUVCoords |
    aiProcess_OptimizeMeshes |
    aiProcess_Debone |
    aiProcess_ValidateDataStructure;

struct LogStream : public Assimp::LogStream
{
    static void Initialize()
    {
        if(Assimp::DefaultLogger::isNullLogger())
        {
            Assimp::DefaultLogger::create("AssimpLogger",Assimp::Logger::VERBOSE);
            Assimp::DefaultLogger::get()->attachStream(new LogStream,
                Assimp::Logger::Err | Assimp::Logger::Warn);
        }
    }

    void write(const char* message) override
    {
        std::fprintf(stderr, "Assimp: %s", message);
    }
};

Mesh::Mesh(const aiMesh* Mesh)
{
    assert(Mesh->HasPositions());
    assert(Mesh->HasNormals());

    m_Vertices.reserve(Mesh->mNumVertices);
    for (size_t i = 0; i < m_Vertices.capacity(); ++i) 
    {
        Vertex vertex;
        vertex.Position = { Mesh->mVertices[i].x, Mesh->mVertices[i].y, Mesh->mVertices[i].z };

        vertex.Normal = { Mesh->mNormals[i].x, Mesh->mNormals[i].y, Mesh->mNormals[i].z };

        if (Mesh->HasTangentsAndBitangents()) 
        {
            vertex.Tangent = { Mesh->mTangents[i].x, Mesh->mTangents[i].y, Mesh->mTangents[i].z };
            vertex.BiTangent = { Mesh->mBitangents[i].x, Mesh->mBitangents[i].y, Mesh->mBitangents[i].z };
        }

        if (Mesh->HasTextureCoords(0)) 
        {
            vertex.Texcoord = { Mesh->mTextureCoords[0][i].x, Mesh->mTextureCoords[0][i].y };
        }

        m_Vertices.push_back(vertex);
    }

    m_Faces.reserve(Mesh->mNumFaces);
    for (size_t i = 0; i < m_Faces.capacity(); ++i) 
    {
        assert(Mesh->mFaces[i].mNumIndices == 3);
        m_Faces.push_back({ Mesh->mFaces[i].mIndices[0], Mesh->mFaces[i].mIndices[1], Mesh->mFaces[i].mIndices[2] });
    }
}

Mesh::Mesh(MeshData& InMeshData)
{
    m_Vertices.reserve(InMeshData.Vertices.size());
    for (size_t i = 0; i < m_Vertices.capacity(); ++i)
    {
        m_Vertices.push_back(InMeshData.Vertices[i]);
    }

    m_Faces.reserve(InMeshData.Indices32.size() / 3);
    for(size_t i = 0;i < m_Faces.capacity(); ++i)
    {
        size_t Index = i * 3;
        Face face;
        face.v1 = InMeshData.Indices32[Index + 0];
        face.v2 = InMeshData.Indices32[Index + 1];
        face.v3 = InMeshData.Indices32[Index + 2];

        m_Faces.push_back(face);
    }

}

std::shared_ptr<Mesh> Mesh::FromFile(const std::string& FileName)
{
    LogStream::Initialize();
    std::printf("Loading mesh : %s\n", FileName.c_str());

    std::shared_ptr<Mesh>mesh;
    Assimp::Importer Importer;

    const aiScene* scene = Importer.ReadFile(FileName, ImportFlags);
    if(scene && scene->HasMeshes())
    {
        mesh = std::make_shared<Mesh>( Mesh{ scene->mMeshes[0] });
    }
    else
    {
        throw std::runtime_error("Failed to load mesh file:" + FileName);
    }

    return mesh;
}

std::shared_ptr<Mesh> Mesh::FromString(const std::string& data)
{
    LogStream::Initialize();

    std::shared_ptr<Mesh> mesh;
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFileFromMemory(data.c_str(), data.length(), ImportFlags, "nff");

    if (scene && scene->HasMeshes()) 
    {
        mesh = std::shared_ptr<Mesh>(new Mesh{ scene->mMeshes[0] });
    }
    else 
    {
        throw std::runtime_error("Failed to create mesh from string: " + data);
    }
    return mesh;
}

