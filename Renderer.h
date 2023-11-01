#pragma once

#include "HelperStructs.h"
#include "glm/gtc/matrix_transform.hpp"
#include <cassert>

#define VK_CHECK(expr) { if ((expr)) { assert(false && "Vulkan Error"); } }

class Window;

class Renderer
{
public:
	Renderer(Window* window);
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
	VkSwapchainKHR CreateSwapchain() const;

private:
	Window* mWindow;
	VkInstance mInstance = nullptr;
	VkDebugUtilsMessengerEXT mDebugMessenger = 0;
	VkPhysicalDeviceInfo mPhysicalDevice = {};
	VkDevice mDevice = nullptr;
	VkQueue mGraphicsQueue = nullptr;
	VkQueue mTransferQueue = nullptr;
	VkCommandBuffer mMainCmd = nullptr;
	VkCommandPool mMainCmdPool = 0;
	VkCommandBuffer mMainTransferCmd = nullptr;
	VkCommandPool mMainTransferCmdPool = 0;
	VkSwapchainKHR mSwapchain = 0;
};

