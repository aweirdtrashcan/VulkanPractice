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
	VkPipelineLayout CreatePipelineLayout() const;
	VkPipeline CreateVulkanPipeline() const;

private:
	static constexpr int shaderCodeMaxSize = 1024 * 10;

	const Window* mWindow;
	VkInstance mInstance = nullptr;
	VkDebugUtilsMessengerEXT mDebugMessenger = nullptr;
	
	VkPhysicalDeviceInfo mPhysicalDevice = {};
	VkDevice mDevice = nullptr;
	
	VkQueue mGraphicsQueue = nullptr;
	VkQueue mTransferQueue = nullptr;
	
	VkCommandBuffer mMainCmd = nullptr;
	VkCommandPool mMainCmdPool = nullptr;
	VkCommandBuffer mMainTransferCmd = nullptr;
	VkCommandPool mMainTransferCmdPool = nullptr;
	
	VkSurfaceKHR mSurface = nullptr;
	VkSwapchainKHR mSwapchain = nullptr;
	VkSurfaceFormatKHR mSwapchainSurfaceFormat = {};

	std::vector<VkImage> mImages;
	std::vector<VkImageView> mImageViews;
	uint32_t mImageCount = 0;

	VkRenderPass mRenderpass = nullptr;

	VkViewport mViewport = {};
	VkRect2D mScissor = {};

	VkPipelineLayout mPipelineLayout = nullptr;
	VkPipeline mGraphicsPipeline = nullptr;
};

