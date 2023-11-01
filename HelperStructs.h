#pragma once

#include <vulkan/vulkan.h>
#include <vector>

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