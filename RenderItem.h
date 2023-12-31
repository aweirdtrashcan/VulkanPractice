#pragma once

#include <cstdint>
#include "HelperStructs.h"

struct RenderItem
{
	// The uniform buffer index can be determined as the index of the renderitem in the array of RIs.

	struct MeshGeometry* MeshGeo;
	
	SingleObjectUniform UniformBuffer;

	bool updated = false;

	uint32_t uniformBufferIndex;

	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t vertexOffset;
};

