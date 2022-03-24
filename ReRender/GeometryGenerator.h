#pragma once
#include <cstdint>
#include <vector>

#include "Mesh.h"

struct MeshData
{
	std::vector<Vertex> Vertices;
	std::vector<uint32_t> Indices32;
};

class GeometryGenerator
{
public:
	MeshData CreateQuad(float x, float y, float w, float h, float depth);
};

