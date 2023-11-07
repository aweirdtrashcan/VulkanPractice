#pragma once

#include "HelperStructs.h"
#include "glm/gtc/matrix_transform.hpp"
#include <cassert>

#define VK_CHECK(expr) { if ((expr)) { assert(false && "Vulkan Error"); } }

class Window;

class Renderer
{
public:
	Renderer(const Window* window);
	~Renderer();
	
	void StopRender() {};
	void NotifyWindowResize() {};
	void ToggleVSync() {};

	void Update();
	void Draw();

private:
	VkInstance CreateVulkanInstance() const;
	VkDebugUtilsMessengerEXT CreateVulkanMessenger() const;
	VkPhysicalDeviceInfo ChoosePhysicalDevice() const;
	VkDeviceInfo CreateLogicalDevice() const;
	VkCommandPool CreateCommandPool(uint32_t queueFamilyIndex) const;
	VkCommandBuffer AllocateCommandBuffer(VkCommandPool commandPool) const;
	VkSurfaceKHR CreateVulkanSurface() const;
	VkSwapchainKHR CreateSwapchain(VkSurfaceFormatKHR& out_swapchainSurfaceFormat) const;
	uint32_t GetSwapchainImagesCount() const;
	std::vector<VkImage> GetSwapchainImages(uint32_t imageCount) const;
	VkImageView CreateImageView(VkFormat viewFormat, VkImage image, VkImageAspectFlags imageAspect) const;
	VkShaderModule CreateShaderModule(const char* shaderPath) const;
	VkRenderPass CreateRenderPass() const;
	VkFramebuffer CreateFramebuffer(VkRenderPass renderpass, uint32_t numImageViews, VkImageView* imageViews, uint32_t width, uint32_t height) const;
	VkPipelineLayout CreatePipelineLayout() const;
	VkPipeline CreateVulkanPipeline() const;
	VkFence CreateVulkanFence() const;
	VkSemaphore CreateSemaphore() const;
	int32_t FindMemoryIndex(uint32_t memoryTypeBits, VkMemoryPropertyFlags requestedMemoryType) const;
	Buffer CreateBuffer(VkBufferUsageFlags bufferUsage, VkDeviceSize bufferSize, bool cpuAccessible) const;
	Buffer CreateUploadBuffer(VkDeviceSize bufferSize) const;
	void UploadToBuffer(Buffer& destinationBuffer, Buffer& uploadBuffer, const void* data, VkDeviceSize bufferSize);
	void BindBuffer(const Buffer& buffer, VkDeviceSize offset) const;
	inline void BindBuffer(const Buffer& buffer) const { BindBuffer(buffer, 0); }
	void DestroyBuffer(Buffer* buffer) const;

private:
	static constexpr int shaderCodeMaxSize = 1024 * 10;

	const Window* mWindow;
	VkInstance mInstance = nullptr;
	VkDebugUtilsMessengerEXT mDebugMessenger = nullptr;
	
	VkPhysicalDeviceInfo mPhysicalDevice = {};
	VkDevice mDevice = nullptr;
	uint32_t mGraphicsQueueIndex = 0;
	
	VkQueue mGraphicsQueue = nullptr;
	VkQueue mTransferQueue = nullptr;
	
	VkCommandBuffer mMainCmd = nullptr;
	VkCommandPool mMainCmdPool = nullptr;
	VkCommandBuffer mMainTransferCmd = nullptr;
	VkCommandPool mMainTransferCmdPool = nullptr;
	
	VkFence mMainCopyFence = nullptr;
	VkSemaphore mMainCopyDoneSemaphore = nullptr;

	uint32_t mImageIndex = 0;

	VkSurfaceKHR mSurface = nullptr;
	VkSwapchainKHR mSwapchain = nullptr;
	VkSurfaceFormatKHR mSwapchainSurfaceFormat = {};

	std::vector<VkImage> mImages;
	std::vector<VkImageView> mImageViews;
	std::vector<bool> mIsImageFirstTime;
	uint32_t mImageCount = 0;

	std::vector<FrameResources> mFrameResources;

	VkRenderPass mRenderpass = nullptr;

	VkViewport mViewport = {};
	VkRect2D mScissor = {};

	Buffer mVertexBuffer;
	Buffer mIndexBuffer;

	uint32_t mIndexCount = 0;

	VkPipelineLayout mPipelineLayout = nullptr;
	VkPipeline mGraphicsPipeline = nullptr;
};

