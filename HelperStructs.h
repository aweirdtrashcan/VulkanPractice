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

struct Buffer
{
	VkBuffer buffer;
	VkDeviceMemory memory;
	uint32_t size;
};

struct FrameResources
{
	VkSemaphore ImageAcquired;
	VkSemaphore ImagePresented;
	VkFence Fence;
	
	VkCommandBuffer CommandBuffer;
	VkCommandPool CommandPool;
	VkFramebuffer Framebuffer;

	VkDescriptorSet GlobalDescriptorSet;
};

struct GlobalUniform 
{
	glm::mat4 projection;
	glm::mat4 view;
	glm::mat4 tempModel;
	glm::mat4 padding[1];
};

struct SingleObjectUniform 
{
	glm::mat4 mvp;
};

inline uint64_t CalculateConstantBufferSize(uint64_t bufferSize) { return (bufferSize + 255) & ~255; }