#include "Renderer.h"

#include "Window.h"

#include <vector>
#include <iostream>

VkBool32 __stdcall vkDebugUtilsMessengerCallback (
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {
	std::cout << pCallbackData->pMessage << "\n";
	return VK_FALSE;
}

Renderer::Renderer(Window* window)
	:
	mWindow(window)
{
	mInstance = CreateVulkanInstance();
	mDebugMessenger = CreateVulkanMessenger();
	mPhysicalDevice = ChoosePhysicalDevice();
	mDevice = CreateLogicalDevice();
}

Renderer::~Renderer()
{
	{
		/* DebugUtilsMessenger */
		PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)
			vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT");
		if (func && mDebugMessenger)
			func(mInstance, mDebugMessenger, nullptr);
	}

	if (mInstance) vkDestroyInstance(mInstance, nullptr);
}

VkInstance Renderer::CreateVulkanInstance() const
{
	VkApplicationInfo appInfo = {};
	VkInstance instance = nullptr;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
	appInfo.engineVersion = appInfo.applicationVersion;
	uint32_t instanceVersion = 0;
	VK_CHECK(vkEnumerateInstanceVersion(&instanceVersion))
	appInfo.apiVersion = instanceVersion;
	appInfo.pEngineName = "Vulkan Engine";
	appInfo.pApplicationName = "Vulkan Application";
	
	uint32_t supportedExtensionsCount = 0;
	uint32_t supportedLayersCount = 0;

	VK_CHECK(vkEnumerateInstanceExtensionProperties(
		nullptr,
		&supportedExtensionsCount,
		nullptr
	));

	VK_CHECK(vkEnumerateInstanceLayerProperties(
		&supportedLayersCount,
		nullptr
	));

	std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionsCount);
	std::vector<VkLayerProperties> supportedLayers(supportedLayersCount);

	VK_CHECK(vkEnumerateInstanceExtensionProperties(
		nullptr,
		&supportedExtensionsCount,
		supportedExtensions.data()
	));

	VK_CHECK(vkEnumerateInstanceLayerProperties(
		&supportedLayersCount,
		supportedLayers.data()
	));

	std::cout << "Supported Instance Extensions:\n";
	for (const VkExtensionProperties& ext : supportedExtensions)
	{
		std::cout << "\t" << ext.extensionName << "\n";
	}

	std::cout << "Supported Instance Layers:\n";
	for (const VkLayerProperties& layer : supportedLayers)
	{
		std::cout << "\t" << layer.layerName << "\n";
	}

	std::vector<const char*> requestedExtensions;
	std::vector<const char*> requestedLayers;

	requestedExtensions.push_back("VK_KHR_surface");
	requestedExtensions.push_back("VK_KHR_win32_surface");
	requestedExtensions.push_back("VK_EXT_debug_utils");

	requestedLayers.push_back("VK_LAYER_KHRONOS_validation");

	for (const char* reqExt : requestedExtensions)
	{
		bool supported = false;
		for (const VkExtensionProperties& ext : supportedExtensions)
		{
			if (strcmp(reqExt, ext.extensionName) == 0)
			{
				std::cout << reqExt << " is supported!\n";
				supported = true;
				break;
			}
		}
		if (!supported)
		{
			std::cout << reqExt << " is NOT supported!\n";
			throw std::exception("One or more instance extensions requested are not supported by your GPU.");
		}
	}

	for (const char* reqLay : requestedLayers)
	{
		bool supported = false;
		for (const VkLayerProperties& lay : supportedLayers)
		{
			if (strcmp(reqLay, lay.layerName) == 0)
			{
				std::cout << reqLay << " is supported!\n";
				supported = true;
				break;
			}
		}
		if (!supported)
		{
			std::cout << reqLay << " is NOT supported!\n";
			if (reqLay != "VK_LAYER_KHRONOS_validation")
				throw std::exception("One or more instance extensions requested are not supported by your GPU.");
		}
	}

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = requestedExtensions.data();

	VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

	std::cout << "Instance created successfully\n";

	return instance;
}

VkDebugUtilsMessengerEXT Renderer::CreateVulkanMessenger() const
{
	VkDebugUtilsMessengerEXT messenger = 0;
	VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {};
	messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messengerCreateInfo.pfnUserCallback = vkDebugUtilsMessengerCallback;
	messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
									  VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
									  VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	messengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
										  VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
										  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT");

	if (func)
		VK_CHECK(func(mInstance, &messengerCreateInfo, nullptr, &messenger));

	return messenger;
}

VkPhysicalDeviceInfo Renderer::ChoosePhysicalDevice() const
{
	VkPhysicalDeviceInfo physicalDeviceInfo = {};
	uint32_t physicalDevicesCount = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(mInstance, &physicalDevicesCount, nullptr));
	std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
	VK_CHECK(vkEnumeratePhysicalDevices(mInstance, &physicalDevicesCount, physicalDevices.data()));

	uint64_t physicalDeviceMaxMemory = 0;
	for (VkPhysicalDevice physicalDevice : physicalDevices)
	{
		VkPhysicalDeviceProperties properties = {};
		VkPhysicalDeviceMemoryProperties memory = {};
		VkPhysicalDeviceFeatures features = {};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memory);
		vkGetPhysicalDeviceFeatures(physicalDevice, &features);
		uint32_t queueFamilyPropertiesCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(
			physicalDevice, 
			&queueFamilyPropertiesCount, 
			nullptr
		);
		std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertiesCount);
		vkGetPhysicalDeviceQueueFamilyProperties(
			physicalDevice,
			&queueFamilyPropertiesCount,
			queueFamilyProperties.data()
		);

		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
		{
			std::wcout << "Found a Device: " << properties.deviceName << "\n";

			uint64_t maxMemory = 0;
			for (uint32_t i = 0; i < memory.memoryHeapCount; i++)
			{
				maxMemory += memory.memoryHeaps[i].size;
			}

			if (maxMemory > physicalDeviceMaxMemory)
			{
				physicalDeviceInfo.physicalDevice = physicalDevice;
				physicalDeviceInfo.memoryProperties = memory;
				physicalDeviceInfo.properties = properties;
				physicalDeviceInfo.features = features;
				physicalDeviceInfo.queueFamilyProperties = queueFamilyProperties;
				physicalDeviceMaxMemory = maxMemory;
			}
		}
	}

	std::wcout << "Device chosen is: " << physicalDeviceInfo.properties.deviceName << "\n";

	return physicalDeviceInfo;
}

VkDevice Renderer::CreateLogicalDevice() const
{
	VkDevice device = nullptr;
	uint32_t supportedExtensionsCount = 0;

	VK_CHECK(vkEnumerateDeviceExtensionProperties(
		mPhysicalDevice.physicalDevice,
		nullptr,
		&supportedExtensionsCount,
		nullptr
	));

	std::vector<VkExtensionProperties> supportedExtensions(supportedExtensionsCount);

	VK_CHECK(vkEnumerateDeviceExtensionProperties(
		mPhysicalDevice.physicalDevice,
		nullptr,
		&supportedExtensionsCount,
		supportedExtensions.data()
	));

	std::cout << "Device supported Extensions:\n";
	for (VkExtensionProperties& ext : supportedExtensions)
	{
		std::cout << "\t" << ext.extensionName << "\n";
	}

	std::vector<const char*> requestedExtensions;

	requestedExtensions.push_back("VK_KHR_swapchain");

	for (const char* reqExt : requestedExtensions)
	{
		bool found = false;
		for (const VkExtensionProperties& supExt : supportedExtensions)
		{
			if (strcmp(reqExt, supExt.extensionName) == 0)
			{
				std::cout << supExt.extensionName << " is supported!\n";
				found = true;
				break;
			}
		}
		if (!found)
		{
			std::cout << reqExt << " is not supported!\n";
			throw std::exception("One or more device extensions are not supported.");
		}
	}

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size());
	createInfo.ppEnabledExtensionNames = requestedExtensions.data();
	
	/* Find each queue index */

	uint32_t graphicsQueueIndex = -1;
	uint32_t graphicsQueueIndexCandidate = -1;
	uint32_t transferQueueIndex = -1;
	uint32_t transferQueueIndexCandidate = -1;

	for (uint32_t i = 0; i < mPhysicalDevice.queueFamilyProperties.size(); i++)
	{
		bool isGraphics = mPhysicalDevice.queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
		bool isTransfer = mPhysicalDevice.queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT;

		if (isGraphics && graphicsQueueIndex == -1)
		{
			if (!isTransfer)
			{
				graphicsQueueIndex = i;
			}
			else
			{
				graphicsQueueIndexCandidate = i;
			}
		}

		if (isTransfer && transferQueueIndex == -1)
		{
			if (!isGraphics)
			{
				transferQueueIndex = i;
			}
			else
			{
				transferQueueIndexCandidate = i;
			}
		}
	}

	if (graphicsQueueIndex == -1)
		graphicsQueueIndex = graphicsQueueIndexCandidate;

	if (transferQueueIndex == -1)
		transferQueueIndex = transferQueueIndexCandidate;

	float queuePriorities[1] = { 1.0f };

	VkDeviceQueueCreateInfo queueCreateInfo[2];
	queueCreateInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo[0].queueFamilyIndex = graphicsQueueIndex;
	queueCreateInfo[0].queueCount = 1;
	queueCreateInfo[0].pQueuePriorities = queuePriorities;
	queueCreateInfo[0].pNext = nullptr;
	queueCreateInfo[0].flags = 0;

	queueCreateInfo[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo[1].queueFamilyIndex = transferQueueIndex;
	queueCreateInfo[1].queueCount = 1;
	queueCreateInfo[1].pQueuePriorities = queuePriorities;
	queueCreateInfo[1].pNext = nullptr;
	queueCreateInfo[1].flags = 0;

	createInfo.queueCreateInfoCount = std::size(queueCreateInfo);
	createInfo.pQueueCreateInfos = queueCreateInfo;
	
	VK_CHECK(vkCreateDevice(mPhysicalDevice.physicalDevice, &createInfo, nullptr, &device));

	return device;
}
