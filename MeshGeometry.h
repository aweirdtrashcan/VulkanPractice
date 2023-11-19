#pragma once

#include "HelperStructs.h"
#include <unordered_map>

struct SubmeshGeometry
{
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t vertexOffset;
};

struct MeshGeometry
{
	std::unordered_map<const char*, SubmeshGeometry> Geometries;

	Buffer VertexBuffer;
	Buffer IndexBuffer;
};
