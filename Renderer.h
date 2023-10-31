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
	VkDevice CreateLogicalDevice() const;

private:
	Window* mWindow;
	VkInstance mInstance = nullptr;
	VkDebugUtilsMessengerEXT mDebugMessenger = 0;
	VkPhysicalDeviceInfo mPhysicalDevice = {};
	VkDevice mDevice = nullptr;
};

