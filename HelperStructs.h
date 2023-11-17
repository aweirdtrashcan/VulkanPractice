#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <DirectXMath.h>

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
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
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
	Buffer* ObjectUniformBuffer;
	std::vector<VkDescriptorSet> ObjectDescriptorSet;
};

struct GlobalUniform 
{
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 invView;
	DirectX::XMFLOAT4X4 projection;
	DirectX::XMFLOAT4X4 invProjection;
	DirectX::XMFLOAT4X4 viewProj;
	DirectX::XMFLOAT4X4 invViewProj;
	DirectX::XMFLOAT3 eyePosW;
	float perObjectPad1;
	DirectX::XMFLOAT2 renderTargetSize;
	float nearZ;
	float farZ;
	float totalTime;
	float deltaTime;
};

struct SingleObjectUniform 
{
	DirectX::XMFLOAT4X4 model;
};

inline constexpr uint64_t CalculateUniformBufferSize(uint64_t bufferSize) { return (bufferSize + 255) & ~255; }