#pragma once

#include <unordered_map>

struct SubmeshGeometry
{
	uint32_t indexCount;
	uint32_t firstVertex;
	uint32_t vertexOffset;
};

struct MeshGeometry
{
	std::unordered_map<const char*, SubmeshGeometry> Geometries;
};

