#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <glm/include/glm/glm.hpp>


class Mesh
{
    using Vec3 = glm::vec3;
    using Vec2 = glm::vec2;

public:

    struct Vertex
    {
        Vec3 Position; 
        Vec3 Normal;
        Vec3 Tangent;
        Vec3 BiTangent;
        Vec2 Texcoord;
    };

    static_assert(sizeof(Vertex) == 14 * sizeof(float));
    static const int NumAttributes = 5;

    struct Face
    {
        uint32_t v1, v2, v3;
    };
    static_assert(sizeof(Face) == 3 * sizeof(uint32_t));

    static std::shared_ptr<Mesh> FromFile(const std::string& FileName);
    static std::shared_ptr<Mesh> FromString(const std::string& Data);

    const std::vector<Vertex>& Vertices() const { return m_Vertices; }
    const std::vector<Face>& Faces()const { return m_Faces; }

private:

    Mesh(const struct aiMesh* Mesh);
    std::vector<Vertex> m_Vertices;
    std::vector<Face> m_Faces;

};

