#pragma once
#include <cstdint>
#include <vector>

#include "Mesh.h"

class GeometryGenerator
{
public:
	static MeshData CreateQuad(float x, float y, float w, float h, float depth);
};

