#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <glm/matrix.hpp>

struct VkPhysicalDeviceInfo
{
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties properties;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkPhysicalDeviceFeatures features;
	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
};

struct VkDeviceInfo
{
	VkDevice device;
	uint32_t graphicsQueueIndex;
	uint32_t transferQueueIndex;
};

struct VertexBufferModel
{
	glm::vec3 pos;
	glm::vec4 color;
};

struct FrameResources
{

};