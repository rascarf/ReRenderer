#include "GeometryGenerator.h"

MeshData GeometryGenerator::CreateQuad(float x, float y, float w, float h, float depth)
{
    MeshData meshData;
    meshData.Vertices.resize(4);
    meshData.Indices32.resize(6);
    

    //直接在NDC Space创建
    meshData.Vertices[0] = Vertex(
        Vec3(x,y - h,depth),
        Vec3(0.0f,0.0f,-1.0f),
        Vec3(1.0f,0.0f,0.0f),
        Vec3(0.0f,1.0f,1.0f),
        Vec2(0.0,1.0f)
    );

    meshData.Vertices[1] = Vertex(
        Vec3(x, y, depth),
        Vec3(0.0f, 0.0f, -1.0f),
        Vec3(1.0f, 0.0f, 0.0f),
        Vec3(0.0f, 1.0f, 1.0f),
        Vec2(0.0, 0.0f)
    );

    meshData.Vertices[2] = Vertex(
        Vec3(x + w, y, depth),
        Vec3(0.0f, 0.0f, -1.0f),
        Vec3(1.0f, 0.0f, 0.0f),
        Vec3(0.0f, 1.0f, 1.0f),
        Vec2(1.0, 0.0f)
    );

    meshData.Vertices[3] = Vertex(
        Vec3(x +w, y - h, depth),
        Vec3(0.0f, 0.0f, -1.0f),
        Vec3(1.0f, 0.0f, 0.0f),
        Vec3(0.0f, 1.0f, 1.0f),
        Vec2(1.0, 1.0f)
    );

    meshData.Indices32[0] = 0;
    meshData.Indices32[1] = 1;
    meshData.Indices32[2] = 2;

    meshData.Indices32[3] = 0;
    meshData.Indices32[4] = 2;
    meshData.Indices32[5] = 3;

    return meshData;
}
