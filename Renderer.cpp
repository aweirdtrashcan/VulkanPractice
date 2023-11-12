#include "Renderer.h"

#include "Window.h"

#include "vulkan/vulkan_win32.h"

#include <vector>
#include <iostream>

using namespace DirectX;

// stupid windows macro
#ifdef CreateSemaphore
#undef CreateSemaphore
#endif

// statically allocated buffer for log writing
#define MSG_BUF_SIZE (2048)

static char* msgBuf;

VkBool32 __stdcall vkDebugUtilsMessengerCallback (
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {
	ZeroMemory(msgBuf, MSG_BUF_SIZE);
	sprintf_s(msgBuf, MSG_BUF_SIZE, "%s:%s\n\n", pCallbackData->pMessageIdName, pCallbackData->pMessage);
	printf("%s", msgBuf);
	OutputDebugStringA(msgBuf);
	return VK_FALSE;
}

Renderer::Renderer(const Window* window)
	:
	mWindow(window)
{
	mTimer.MarkTime();
	msgBuf = (char*)malloc(MSG_BUF_SIZE);
	mInstance = CreateVulkanInstance();
	mDebugMessenger = CreateVulkanMessenger();
	mPhysicalDevice = ChoosePhysicalDevice();
	VkDeviceInfo deviceInfo = CreateLogicalDevice();
	mDevice = deviceInfo.device;
	mGraphicsQueueIndex = deviceInfo.graphicsQueueIndex;
	vkGetDeviceQueue(mDevice, deviceInfo.graphicsQueueIndex, 0, &mGraphicsQueue);
	vkGetDeviceQueue(mDevice, deviceInfo.transferQueueIndex, 0, &mTransferQueue);
	mMainCmdPool = CreateCommandPool(deviceInfo.graphicsQueueIndex);
	mMainCmd = AllocateCommandBuffer(mMainCmdPool);
	mMainTransferCmdPool = CreateCommandPool(deviceInfo.transferQueueIndex);
	mMainTransferCmd = AllocateCommandBuffer(mMainTransferCmdPool);
	mSurface = CreateVulkanSurface();
	mSwapchain = CreateSwapchain(mSwapchainSurfaceFormat);
	mImageCount = GetSwapchainImagesCount();
	mImages = GetSwapchainImages(mImageCount);
	mRenderpass = CreateRenderPass();

	for (VkImage image : mImages)
	{
		VkImageView imgView = CreateImageView(mSwapchainSurfaceFormat.format, image, VK_IMAGE_ASPECT_COLOR_BIT);
		mImageViews.push_back(imgView);
		
		FrameResources frameRes;
		frameRes.ImageAcquired = CreateSemaphore();
		frameRes.ImagePresented = CreateSemaphore();
		frameRes.Fence = CreateVulkanFence();
		frameRes.CommandPool = CreateCommandPool(deviceInfo.graphicsQueueIndex);
		frameRes.CommandBuffer = AllocateCommandBuffer(frameRes.CommandPool);
		frameRes.Framebuffer = CreateFramebuffer(mRenderpass, 1, &imgView, mWindow->GetWindowWidth(), mWindow->GetWindowHeight());
		mFrameResources.push_back(frameRes);
	}

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		mPhysicalDevice.physicalDevice,
		mSurface,
		&surfaceCapabilities
	));

	mViewport.minDepth = 0.0f;
	mViewport.maxDepth = 1.0f;
	mViewport.width = static_cast<float>(surfaceCapabilities.currentExtent.width);
	mViewport.height = -static_cast<float>(surfaceCapabilities.currentExtent.height);
	mViewport.x = 0.0f;
	mViewport.y = static_cast<float>(surfaceCapabilities.currentExtent.height);

	mScissor.extent = surfaceCapabilities.currentExtent;
	mScissor.offset = { 0, 0 };

	mGlobalDescriptorSetLayout = CreateDescriptorSetLayout();
	mGlobalDescriptorPool = CreateDescriptorPool();
	std::vector<VkDescriptorSet> descriptorSets = AllocateGlobalDescriptorSets();

	/* Start uniform buffer */
	mGlobalUniformBuffer = CreateGlobalUniformBuffer(mImageCount);
	BindBuffer(mGlobalUniformBuffer);

	mMeshGeometry = CreateMeshGeometry();

	RenderItem renderItem;
	renderItem.firstIndex = mMeshGeometry.Geometries["Triangle"].firstIndex;
	renderItem.indexCount = mMeshGeometry.Geometries["Triangle"].indexCount;
	renderItem.vertexOffset = mMeshGeometry.Geometries["Triangle"].vertexOffset;
	renderItem.MeshGeo = &mMeshGeometry;
	renderItem.UniformBuffer.model = XMFLOAT4X4();
	renderItem.UniformBuffer.mvp = XMFLOAT4X4();
	mRenderItems.push_back(renderItem);

	RenderItem renderItem2;
	renderItem2.firstIndex = mMeshGeometry.Geometries["Square"].firstIndex;
	renderItem2.indexCount = mMeshGeometry.Geometries["Square"].indexCount;
	renderItem2.vertexOffset = mMeshGeometry.Geometries["Square"].vertexOffset;
	renderItem2.MeshGeo = &mMeshGeometry;
	renderItem2.UniformBuffer.model = XMFLOAT4X4();
	renderItem2.UniformBuffer.mvp = XMFLOAT4X4();
	mRenderItems.push_back(renderItem2);

	uint64_t renderItemCount = mRenderItems.size();

	Buffer* ObjectUniformBuffer = new Buffer;
	*ObjectUniformBuffer = CreateUniformBuffer((renderItemCount * mImageCount) * CalculateUniformBufferSize(sizeof(renderItem.UniformBuffer)));

	BindBuffer(*ObjectUniformBuffer);

	for (size_t i = 0; i < mFrameResources.size(); i++) {
		mFrameResources[i].GlobalDescriptorSet = descriptorSets[i];
		UpdateDescriptorSet(mGlobalUniformBuffer, sizeof(GlobalUniform), descriptorSets[i], i, 0);
		mFrameResources[i].ObjectUniformBuffer = ObjectUniformBuffer;
		for (uint64_t j = 0; j < renderItemCount; j++) {
			VkDescriptorSet descriptorSet = CreateDescriptorSet();
			mFrameResources[i].ObjectDescriptorSet.push_back(descriptorSet);
			UpdateDescriptorSet(*ObjectUniformBuffer, CalculateUniformBufferSize(sizeof(SingleObjectUniform)), descriptorSet, i * renderItemCount + j, 0);
		}
	}
	
	/* End uniform buffer */

	mPipelineLayout = CreatePipelineLayout();
	mGraphicsPipeline = CreateVulkanPipeline();

	static float aspectRatio = (float)mWindow->GetWindowWidth() / (float)mWindow->GetWindowHeight();
	XMMATRIX perspective = XMMatrixPerspectiveFovLH(45.f, aspectRatio, 0.1, 100.f);
	XMStoreFloat4x4(&mGlobalUniform.projection, perspective);

	XMVECTOR eye = { 0.0f, 0.0f, -3.0f, 0.0f };
	XMVECTOR center = { 0.0f, 0.0f, 0.0f, 0.0f };
	XMVECTOR up = { 0.0f, 1.0f, 0.0f, 0.0f };

	XMMATRIX lookAt = XMMatrixLookAtLH(eye, center, up);
	XMStoreFloat4x4(&mGlobalUniform.view, lookAt);
}

Renderer::~Renderer()
{
	vkDeviceWaitIdle(mDevice);
	for (FrameResources& frameRes : mFrameResources) {
		vkFreeDescriptorSets(mDevice, mGlobalDescriptorPool, 1u, &frameRes.GlobalDescriptorSet);
		vkDestroySemaphore(mDevice, frameRes.ImageAcquired, nullptr);
		vkDestroySemaphore(mDevice, frameRes.ImagePresented, nullptr);
		vkDestroyFence(mDevice, frameRes.Fence, nullptr);
		vkDestroyFramebuffer(mDevice, frameRes.Framebuffer, nullptr);
		vkFreeCommandBuffers(mDevice, frameRes.CommandPool, 1u, &frameRes.CommandBuffer);
		vkDestroyCommandPool(mDevice, frameRes.CommandPool, nullptr);
		DestroyBuffer(frameRes.ObjectUniformBuffer);
	}
	delete mFrameResources[0].ObjectUniformBuffer;
	vkDestroyDescriptorSetLayout(mDevice, mGlobalDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(mDevice, mGlobalDescriptorPool, nullptr);
	DestroyBuffer(&mMeshGeometry.IndexBuffer);
	DestroyBuffer(&mMeshGeometry.VertexBuffer);
	DestroyBuffer(&mGlobalUniformBuffer);
	vkDestroyRenderPass(mDevice, mRenderpass, nullptr);
	vkDestroyPipelineLayout(mDevice, mPipelineLayout, nullptr);
	vkDestroyPipeline(mDevice, mGraphicsPipeline, nullptr);
	for (VkImageView imageView : mImageViews)
		vkDestroyImageView(mDevice, imageView, nullptr);
	vkDestroySwapchainKHR(mDevice, mSwapchain, nullptr);
	vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
	vkFreeCommandBuffers(mDevice, mMainCmdPool, 1u, &mMainCmd);
	vkDestroyCommandPool(mDevice, mMainCmdPool, nullptr);
	vkFreeCommandBuffers(mDevice, mMainTransferCmdPool, 1u, &mMainTransferCmd);
	vkDestroyCommandPool(mDevice, mMainTransferCmdPool, nullptr);
	vkDestroyDevice(mDevice, nullptr);
#ifdef _DEBUG
	PFN_vkDestroyDebugUtilsMessengerEXT func = (PFN_vkDestroyDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(mInstance, "vkDestroyDebugUtilsMessengerEXT");
	func(mInstance, mDebugMessenger, nullptr);
#endif
	vkDestroyInstance(mInstance, nullptr);
	free(msgBuf);
	printf("%s\n", "Goodbyeeeeee =)");
}

void Renderer::Update()
{
	if (!mCanRender) return;
	mImgAcq = mFrameResources[mImageIndex].ImageAcquired;
	VK_CHECK(vkAcquireNextImageKHR(
		mDevice,
		mSwapchain,
		UINT64_MAX,
		mImgAcq,
		nullptr,
		&mImageIndex
	));

	double deltaTime = mTimer.GetDelta();
	mAccumulatedDelta += deltaTime;
	mFps++;
	if (mAccumulatedDelta >= 1.0)
	{
		char fpsString[50];
		snprintf(fpsString, sizeof(fpsString), "Vulkan Application | FPS: %d", mFps);
		mWindow->ChangeWindowTitle(fpsString);
		mAccumulatedDelta = 0.0;
		mFps = 0;
	}

	float pos[] = { -1.0f, 1.0f };

	static float rotation = 0.0f;
	if (rotation >= XM_2PI) {
		rotation = 0.0f;
	}

	rotation += 2.0f * (float)deltaTime;

	for (size_t i = 0; i < mRenderItems.size(); i++) {
		RenderItem& rItem = mRenderItems[i];

		XMMATRIX model = XMMatrixRotationZ(rotation) * XMMatrixTranslation(pos[i], 0.0f, 0.0f);
		XMMATRIX mvp = model * XMLoadFloat4x4(&mGlobalUniform.view) * XMLoadFloat4x4(&mGlobalUniform.projection);

		XMStoreFloat4x4(&rItem.UniformBuffer.mvp, mvp);
		XMStoreFloat4x4(&rItem.UniformBuffer.model, model);
		UpdateUniformBuffer(*mFrameResources[mImageIndex].ObjectUniformBuffer, sizeof(SingleObjectUniform), (uint64_t)mImageIndex * mRenderItems.size() + i, &rItem.UniformBuffer);
	}

	UpdateUniformBuffer(mGlobalUniformBuffer, sizeof(GlobalUniform), (uint64_t)mImageIndex, &mGlobalUniform);
}

void Renderer::Draw()
{
	if (!mCanRender) return;

	VkFence fence = mFrameResources[mImageIndex].Fence;
	VkSemaphore imgPrst = mFrameResources[mImageIndex].ImagePresented;
	VkCommandBuffer cmdBuf = mFrameResources[mImageIndex].CommandBuffer;
	VkCommandPool cmdPool = mFrameResources[mImageIndex].CommandPool;
	VkFramebuffer frameBuffer = mFrameResources[mImageIndex].Framebuffer;

	VK_CHECK(vkWaitForFences(mDevice, 1u, &fence, VK_TRUE, UINT64_MAX));

	VK_CHECK(vkResetFences(mDevice, 1u, &fence));

	VK_CHECK(vkResetCommandPool(mDevice, cmdPool, 0));
	VK_CHECK(vkResetCommandBuffer(cmdBuf, 0));

	VkCommandBufferBeginInfo cmdBeginInfo = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		nullptr,
		0,
		nullptr
	};

	VK_CHECK(vkBeginCommandBuffer(cmdBuf, &cmdBeginInfo));

	VkClearValue clearValue;
	clearValue.color.float32[0] = 1.0f;
	clearValue.color.float32[1] = 0.0f;
	clearValue.color.float32[2] = 0.0f;
	clearValue.color.float32[3] = 1.0f;

	VkRenderPassBeginInfo renderPassBeginInfo;
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = nullptr;
	renderPassBeginInfo.renderPass = mRenderpass;
	renderPassBeginInfo.framebuffer = frameBuffer;
	renderPassBeginInfo.renderArea.extent.width = mWindow->GetWindowWidth();
	renderPassBeginInfo.renderArea.extent.height = mWindow->GetWindowHeight();
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(cmdBuf, 0u, 1u, &mViewport);
	vkCmdSetScissor(cmdBuf, 0u, 1u, &mScissor);

	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

	for (size_t i = 0; i < mRenderItems.size(); i++) {
		RenderItem& rItem = mRenderItems[i];

		VkDescriptorSet descriptorSets[] =
		{
			mFrameResources[mImageIndex].GlobalDescriptorSet,
			mFrameResources[mImageIndex].ObjectDescriptorSet[i]
		};

		vkCmdBindDescriptorSets(
			cmdBuf,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			mPipelineLayout,
			0u,
			(uint32_t)std::size(descriptorSets),
			descriptorSets,
			0u,
			nullptr
		);

		VkDeviceSize s = 0;
		vkCmdBindVertexBuffers(cmdBuf, 0u, 1u, &rItem.MeshGeo->VertexBuffer.buffer, &s);
		vkCmdBindIndexBuffer(cmdBuf, rItem.MeshGeo->IndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(cmdBuf, rItem.indexCount, 1u, rItem.firstIndex, rItem.vertexOffset, 0u);
	}

	vkCmdEndRenderPass(cmdBuf);
	vkEndCommandBuffer(cmdBuf);

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &mImgAcq;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &imgPrst;
	VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.pWaitDstStageMask = &stageFlags;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuf;
	VK_CHECK(vkQueueSubmit(mGraphicsQueue, 1u, &submitInfo, fence));

	VkPresentInfoKHR presentInfo;
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.pSwapchains = &mSwapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &imgPrst;
	presentInfo.pImageIndices = &mImageIndex;
	presentInfo.pResults = 0;
	VK_CHECK(vkQueuePresentKHR(mGraphicsQueue, &presentInfo));
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
			else
				requestedLayers.erase(std::begin(requestedLayers));
		}
	}

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requestedExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = requestedExtensions.data();
	instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requestedLayers.size());
	instanceCreateInfo.ppEnabledLayerNames = requestedLayers.data();

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
										  VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
										  VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	messengerCreateInfo.flags;
	PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(mInstance, "vkCreateDebugUtilsMessengerEXT");

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

VkDeviceInfo Renderer::CreateLogicalDevice() const
{
	VkDeviceInfo deviceInfo;
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
	/* Temporary */
	requestedExtensions.push_back("VK_KHR_maintenance1");

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

	createInfo.queueCreateInfoCount = static_cast<uint32_t>(std::size(queueCreateInfo));
	createInfo.pQueueCreateInfos = queueCreateInfo;

	VK_CHECK(vkCreateDevice(mPhysicalDevice.physicalDevice, &createInfo, nullptr, &deviceInfo.device));

	deviceInfo.graphicsQueueIndex = graphicsQueueIndex;
	deviceInfo.transferQueueIndex = transferQueueIndex;

	return deviceInfo;
}

VkCommandPool Renderer::CreateCommandPool(uint32_t queueFamilyIndex) const
{
	VkCommandPool commandPool = 0;
	VkCommandPoolCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createInfo.queueFamilyIndex = queueFamilyIndex;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	createInfo.pNext = nullptr;

	VK_CHECK(vkCreateCommandPool(mDevice, &createInfo, nullptr, &commandPool));

	return commandPool;
}

VkCommandBuffer Renderer::AllocateCommandBuffer(VkCommandPool commandPool) const
{
	VkCommandBuffer commandBuffer = nullptr;
	VkCommandBufferAllocateInfo allocInfo;
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = 1;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.pNext = nullptr;

	VK_CHECK(vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer));

	return commandBuffer;
}

VkSurfaceKHR Renderer::CreateVulkanSurface() const
{
	VkSurfaceKHR surface = 0;
	VkWin32SurfaceCreateInfoKHR createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.hinstance = mWindow->GetWindowInstance();
	createInfo.hwnd = mWindow->GetHandleToWindow();

	VK_CHECK(vkCreateWin32SurfaceKHR(mInstance, &createInfo, nullptr, &surface));

	return surface;
}

VkSwapchainKHR Renderer::CreateSwapchain(VkSurfaceFormatKHR& out_swapchainSurfaceFormat) const
{
	VkSwapchainKHR swapchain = 0;
	VkSwapchainCreateInfoKHR createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.surface = mSurface;

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		mPhysicalDevice.physicalDevice, 
		mSurface, 
		&surfaceCapabilities
	));

	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	// Some AMD drivers are quite weird, gonna take a better look at it in the future.
	if (imageCount > surfaceCapabilities.maxImageCount)
	{
		imageCount = surfaceCapabilities.maxImageCount;
	}

	createInfo.minImageCount = imageCount;

	uint32_t surfaceFormatsCount = 0;

	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
		mPhysicalDevice.physicalDevice,
		mSurface,
		&surfaceFormatsCount,
		nullptr
	));

	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);

	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(
		mPhysicalDevice.physicalDevice,
		mSurface,
		&surfaceFormatsCount,
		surfaceFormats.data()
	));

	VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_B8G8R8A8_UNORM, VkColorSpaceKHR::VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	bool surfaceFormatSupported = false;

	out_swapchainSurfaceFormat = surfaceFormat;

	std::cout << "Found the following Surface Formats in the GPU:\n";
	for (const VkSurfaceFormatKHR& s : surfaceFormats)
	{
		std::cout << "\tFormat: " << s.format << "\n\tColor Space: " << s.colorSpace << "\n";
		if (surfaceFormat.format == s.format && surfaceFormat.colorSpace == s.colorSpace)
			surfaceFormatSupported = true;
	}
	if (!surfaceFormatSupported)
		throw std::exception("Failed to find a suitable surface format for your GPU.");

	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = surfaceCapabilities.currentExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;
	createInfo.preTransform = surfaceCapabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

	uint32_t presentModesCount = 0;

	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
		mPhysicalDevice.physicalDevice,
		mSurface,
		&presentModesCount,
		nullptr
	));

	std::vector<VkPresentModeKHR> presentModes(presentModesCount);

	VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(
		mPhysicalDevice.physicalDevice,
		mSurface,
		&presentModesCount,
		presentModes.data()
	));

	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const VkPresentModeKHR p : presentModes)
	{
		if (p == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			presentMode = p;
			break;
		}
		else if (p == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = p;
		}

	}

	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = 0;

	VK_CHECK(vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &swapchain));

	return swapchain;
}

uint32_t Renderer::GetSwapchainImagesCount() const
{
	uint32_t imageCount = 0;
	VK_CHECK(vkGetSwapchainImagesKHR(
		mDevice,
		mSwapchain,
		&imageCount,
		nullptr
	));
	return imageCount;
}

std::vector<VkImage> Renderer::GetSwapchainImages(uint32_t imageCount) const
{
	std::vector<VkImage> images(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, images.data()));

	return images;
}

VkImageView Renderer::CreateImageView(VkFormat viewFormat, VkImage image, VkImageAspectFlags imageAspect) const
{
	VkImageView imageView = 0;
	VkImageViewCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	createInfo.format = viewFormat;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.image = image;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	VK_CHECK(vkCreateImageView(mDevice, &createInfo, nullptr, &imageView));

	return imageView;
}

VkShaderModule Renderer::CreateShaderModule(const char* shaderPath) const
{
	HANDLE shaderHandle = CreateFileA(shaderPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);

	DWORD err = GetLastError();

	if (err)
	{
		throw std::exception("Failed to load Shader");
	}

	LPSTR errMessage = 0;

	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS, 
		NULL, 
		err, 
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), 
		(LPSTR)&errMessage, 
		0, 
		nullptr
	);

	std::cout << errMessage << std::endl;

	char* shaderCode = (char*)malloc(shaderCodeMaxSize);
	ZeroMemory(shaderCode, shaderCodeMaxSize);
	DWORD shaderSize = 0;

	BOOL wasRead = ReadFile(shaderHandle, shaderCode, shaderCodeMaxSize, &shaderSize, nullptr);

	if (!wasRead)
	{
		throw std::exception("Failed to read Shader code");
	}

	VkShaderModule shaderModule = 0;

	VkShaderModuleCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.codeSize = static_cast<size_t>(shaderSize);
	createInfo.flags = 0;
	createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode);

	VK_CHECK(vkCreateShaderModule(mDevice, &createInfo, nullptr, &shaderModule));

	free(shaderCode);
	shaderCode = 0;

	return shaderModule;
}

VkRenderPass Renderer::CreateRenderPass() const
{
	VkRenderPass renderPass = nullptr;

	// Just 1 color attachment.
	VkAttachmentDescription attachmentDescription[1];
	attachmentDescription[0].flags = 0;
	attachmentDescription[0].format = mSwapchainSurfaceFormat.format;
	attachmentDescription[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescription[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachments[1];
	colorAttachments[0].attachment = 0;
	colorAttachments[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDesc[1];
	subpassDesc[0].flags = 0;
	subpassDesc[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc[0].inputAttachmentCount = 0;
	subpassDesc[0].pInputAttachments = nullptr;
	subpassDesc[0].colorAttachmentCount = static_cast<uint32_t>(std::size(colorAttachments));
	subpassDesc[0].pColorAttachments = colorAttachments;
	subpassDesc[0].pResolveAttachments = nullptr;
	subpassDesc[0].pDepthStencilAttachment = nullptr;
	subpassDesc[0].preserveAttachmentCount = 0;
	subpassDesc[0].pPreserveAttachments = nullptr;

	VkSubpassDependency subpassDependencies[1];
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies[0].srcAccessMask = 0;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
										   VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dependencyFlags = 0;

	VkRenderPassCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.attachmentCount = static_cast<uint32_t>(std::size(attachmentDescription));
	createInfo.pAttachments = attachmentDescription;
	createInfo.subpassCount = static_cast<uint32_t>(std::size(subpassDesc));
	createInfo.pSubpasses = subpassDesc;
	createInfo.dependencyCount = static_cast<uint32_t>(std::size(subpassDependencies));
	createInfo.pDependencies = subpassDependencies;

	VK_CHECK(vkCreateRenderPass(mDevice, &createInfo, nullptr, &renderPass));

	return renderPass;
}

VkFramebuffer Renderer::CreateFramebuffer(VkRenderPass renderpass, uint32_t numImageViews, VkImageView* imageViews, uint32_t width, uint32_t height) const
{
	VkFramebuffer framebuffer = nullptr;
	VkFramebufferCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.renderPass = renderpass;
	createInfo.attachmentCount = numImageViews;
	createInfo.pAttachments = imageViews;
	createInfo.layers = 1;
	createInfo.width = width;
	createInfo.height = height;

	VK_CHECK(vkCreateFramebuffer(mDevice, &createInfo, nullptr, &framebuffer));
	return framebuffer;
}

Buffer Renderer::CreateGlobalUniformBuffer(uint32_t numFrames) const
{
	constexpr uint64_t uniformBufferSize = 3 * CalculateUniformBufferSize(sizeof(GlobalUniform));
	Buffer buffer;
	VkBufferCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.size = uniformBufferSize;
	createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR;
	createInfo.flags = 0;

	VK_CHECK(vkCreateBuffer(mDevice, &createInfo, nullptr, &buffer.buffer));

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(mDevice, buffer.buffer, &requirements);
	buffer.size = static_cast<uint32_t>(requirements.size);

	uint32_t memIndex = FindMemoryIndex(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	VkMemoryAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.allocationSize = requirements.size;
	allocateInfo.memoryTypeIndex = memIndex;

	VK_CHECK(vkAllocateMemory(mDevice, &allocateInfo, nullptr, &buffer.memory));

	return buffer;
}

VkDescriptorSetLayout Renderer::CreateDescriptorSetLayout() const
{
	VkDescriptorSetLayoutBinding descSetLayoutBinding[1];
	descSetLayoutBinding[0].binding = 0;
	descSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descSetLayoutBinding[0].descriptorCount = 1;
	descSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	descSetLayoutBinding[0].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo;
	descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descSetLayoutCreateInfo.pNext = nullptr;
	descSetLayoutCreateInfo.flags = 0;
	descSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(std::size(descSetLayoutBinding));
	descSetLayoutCreateInfo.pBindings = descSetLayoutBinding;

	VkDescriptorSetLayout descSetLayout = nullptr;

	VK_CHECK(vkCreateDescriptorSetLayout(mDevice, &descSetLayoutCreateInfo, nullptr, &descSetLayout));

	return descSetLayout;
}

VkDescriptorPool Renderer::CreateDescriptorPool() const
{
	VkDescriptorPoolCreateInfo createInfo;
	VkDescriptorPoolSize size;
	size.descriptorCount = 1000;
	size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	VkDescriptorPool descriptorPool = nullptr;

	createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	createInfo.poolSizeCount = 1;
	createInfo.pPoolSizes = &size;
	createInfo.maxSets = 1000;
	
	VK_CHECK(vkCreateDescriptorPool(mDevice, &createInfo, nullptr, &descriptorPool));
	
	return descriptorPool;
}

std::vector<VkDescriptorSet> Renderer::AllocateGlobalDescriptorSets() const
{
	std::vector<VkDescriptorSet> descriptorSets(mImageCount);
	VkDescriptorSetAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.descriptorPool = mGlobalDescriptorPool;
	allocateInfo.descriptorSetCount = mImageCount;
	std::vector<VkDescriptorSetLayout> layouts;
	for (uint32_t i = 0; i < mImageCount; i++)
	{
		layouts.push_back(mGlobalDescriptorSetLayout);
	}
	allocateInfo.pSetLayouts = layouts.data();

	VK_CHECK(vkAllocateDescriptorSets(mDevice, &allocateInfo, descriptorSets.data()));
	return descriptorSets;
}

void Renderer::UpdateUniformBuffer(Buffer buffer, uint64_t bufferStride, uint64_t offset, void* data) const
{
	bufferStride = CalculateUniformBufferSize(bufferStride);
	void* mapped = nullptr;
	VK_CHECK(vkMapMemory(mDevice, buffer.memory, offset * bufferStride, bufferStride, 0, &mapped));
	memcpy(mapped, data, bufferStride);
	vkUnmapMemory(mDevice, buffer.memory);
}

void Renderer::UpdateDescriptorSet(Buffer buffer, uint64_t bufferStride, VkDescriptorSet descriptorSet, uint64_t offset, uint32_t binding) const
{
	VkDescriptorBufferInfo binfo;
	binfo.buffer = buffer.buffer;
	binfo.range = CalculateUniformBufferSize(bufferStride);
	binfo.offset = offset * CalculateUniformBufferSize(bufferStride);

	VkWriteDescriptorSet write;
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.pNext = nullptr;
	write.dstSet = descriptorSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	write.pImageInfo = nullptr;
	write.pBufferInfo = &binfo;
	write.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(mDevice, 1, &write, 0, nullptr);
}

VkPipelineLayout Renderer::CreatePipelineLayout() const
{
	/* Global Descriptor */
	
	VkPipelineLayoutCreateInfo pipeLayoutCreateInfo;
	pipeLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLayoutCreateInfo.pNext = nullptr;
	pipeLayoutCreateInfo.flags = 0;
	VkDescriptorSetLayout layouts[] =
	{
		mGlobalDescriptorSetLayout,
		mGlobalDescriptorSetLayout
	};
	pipeLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(std::size(layouts));
	pipeLayoutCreateInfo.pSetLayouts = layouts;
	pipeLayoutCreateInfo.pushConstantRangeCount = 0;
	pipeLayoutCreateInfo.pPushConstantRanges = nullptr;

	VkPipelineLayout pipelineLayout = nullptr;

	VK_CHECK(vkCreatePipelineLayout(
		mDevice,
		&pipeLayoutCreateInfo,
		nullptr,
		&pipelineLayout
	));

	return pipelineLayout;
}

VkPipeline Renderer::CreateVulkanPipeline() const
{
	VkPipeline pipeline = 0;
	VkGraphicsPipelineCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	createInfo.pNext = nullptr;
#ifdef _DEBUG
	createInfo.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
#else
	createInfo.flags = 0;
#endif

	// Shaders
	VkPipelineShaderStageCreateInfo stages[2];
	
	// Vertex Shader
	VkShaderModule vertexShader = CreateShaderModule("./Shaders/vert.spv");
	VkShaderModule fragShader = CreateShaderModule("./Shaders/frag.spv");

	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].pNext = nullptr;
	stages[0].flags = 0;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertexShader;
	stages[0].pName = "main";
	stages[0].pSpecializationInfo = nullptr;
	
	// Pixel Shader
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].pNext = nullptr;
	stages[1].flags = 0;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragShader;
	stages[1].pName = "main";
	stages[1].pSpecializationInfo = nullptr;

	createInfo.stageCount = static_cast<uint32_t>(std::size(stages));
	createInfo.pStages = stages;

	// Vertex Input
	VkPipelineVertexInputStateCreateInfo vertexInputState;
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputState.pNext = nullptr;
	vertexInputState.flags = 0;

	VkVertexInputBindingDescription vertBindings[1];
	// x,y,z,r,g,b,a
	vertBindings[0].binding = 0;
	vertBindings[0].stride = sizeof(VertexBufferModel);
	vertBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(std::size(vertBindings));
	vertexInputState.pVertexBindingDescriptions = vertBindings;

	VkVertexInputAttributeDescription vertAttributes[2];
	// x,y,z
	vertAttributes[0].binding = 0;
	vertAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertAttributes[0].location = 0;
	vertAttributes[0].offset = 0;

	// r,g,b,a
	vertAttributes[1].binding = 0;
	vertAttributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertAttributes[1].location = 1;
	vertAttributes[1].offset = sizeof(VertexBufferModel::pos);

	vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(std::size(vertAttributes));
	vertexInputState.pVertexAttributeDescriptions = vertAttributes;

	createInfo.pVertexInputState = &vertexInputState;

	// Input Assembly 
	VkPipelineInputAssemblyStateCreateInfo iaCreateInfo;
	iaCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	iaCreateInfo.pNext = nullptr;
	iaCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	iaCreateInfo.primitiveRestartEnable = VK_FALSE;
	iaCreateInfo.flags = 0;

	createInfo.pInputAssemblyState = &iaCreateInfo;

	// Tesselation
	VkPipelineTessellationStateCreateInfo tsCreateInfo;
	tsCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tsCreateInfo.pNext = nullptr;
	tsCreateInfo.flags = 0;
	tsCreateInfo.patchControlPoints = 0;

	createInfo.pTessellationState = &tsCreateInfo;

	// Viewport
	VkPipelineViewportStateCreateInfo vpStateCreateInfo;
	vpStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vpStateCreateInfo.pNext = nullptr;
	vpStateCreateInfo.flags = 0;
	vpStateCreateInfo.pViewports = &mViewport;
	vpStateCreateInfo.pScissors = &mScissor;
	vpStateCreateInfo.scissorCount = vpStateCreateInfo.viewportCount = 1;

	createInfo.pViewportState = &vpStateCreateInfo;

	// Rasterization
	VkPipelineRasterizationStateCreateInfo rsCreateInfo;
	rsCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rsCreateInfo.pNext = nullptr;
	rsCreateInfo.flags = 0;
	rsCreateInfo.depthClampEnable = VK_FALSE;
	rsCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rsCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rsCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rsCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rsCreateInfo.depthBiasEnable = VK_FALSE;
	rsCreateInfo.depthBiasConstantFactor = 0.0f;
	rsCreateInfo.depthBiasClamp = 0.0f;
	rsCreateInfo.depthBiasSlopeFactor = 0.0f;
	rsCreateInfo.lineWidth = 1.0f;

	createInfo.pRasterizationState = &rsCreateInfo;

	// Multisample
	VkPipelineMultisampleStateCreateInfo msCreateInfo;
	msCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msCreateInfo.pNext = nullptr;
	msCreateInfo.flags = 0;
	msCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	msCreateInfo.sampleShadingEnable = VK_FALSE;
	msCreateInfo.minSampleShading = 0.0f;
	msCreateInfo.pSampleMask = nullptr;
	msCreateInfo.alphaToCoverageEnable = VK_FALSE;
	msCreateInfo.alphaToOneEnable = VK_FALSE;

	createInfo.pMultisampleState = &msCreateInfo;

	// DepthStencil
	VkPipelineDepthStencilStateCreateInfo dsCreateInfo;
	dsCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	dsCreateInfo.pNext = nullptr;
	dsCreateInfo.flags = 0;
	dsCreateInfo.depthTestEnable = VK_FALSE;
	dsCreateInfo.depthWriteEnable = VK_FALSE;
	dsCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
	dsCreateInfo.depthBoundsTestEnable = VK_FALSE;
	dsCreateInfo.stencilTestEnable = VK_FALSE;
	dsCreateInfo.front = {};
	dsCreateInfo.back = {};
	dsCreateInfo.minDepthBounds = 0.0f;
	dsCreateInfo.maxDepthBounds = 1.0f;

	createInfo.pDepthStencilState = &dsCreateInfo;

	// ColorBlend
	VkPipelineColorBlendAttachmentState cbAttachments[1];
	cbAttachments[0].blendEnable = VK_TRUE;
	cbAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	cbAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cbAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
	cbAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	cbAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	cbAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
	cbAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_R_BIT |
									  VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;

	VkPipelineColorBlendStateCreateInfo cbCreateInfo;
	cbCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cbCreateInfo.pNext = nullptr;
	cbCreateInfo.flags = 0;
	cbCreateInfo.logicOpEnable = VK_FALSE;
	cbCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	cbCreateInfo.attachmentCount = static_cast<uint32_t>(std::size(cbAttachments));
	cbCreateInfo.pAttachments = cbAttachments;

	createInfo.pColorBlendState = &cbCreateInfo;

	// Dynamic State
	const VkDynamicState dynamicStates[] =
	{
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(std::size(dynamicStates));
	dynamicStateCreateInfo.pDynamicStates = dynamicStates;

	createInfo.pDynamicState = &dynamicStateCreateInfo;

	// Pipeline Layout

	createInfo.layout = mPipelineLayout;

	createInfo.renderPass = mRenderpass;
	createInfo.subpass = 0;
	createInfo.basePipelineHandle = nullptr;
	createInfo.basePipelineIndex = 0;

	VK_CHECK(vkCreateGraphicsPipelines(
		mDevice,
		nullptr,
		1u,
		&createInfo,
		nullptr,
		&pipeline
	));

	vkDestroyShaderModule(mDevice, vertexShader, nullptr);
	vkDestroyShaderModule(mDevice, fragShader, nullptr);

	return pipeline;
}

VkFence Renderer::CreateVulkanFence() const
{
	VkFence fence = nullptr;
	VkFenceCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK(vkCreateFence(
		mDevice,
		&createInfo,
		nullptr,
		&fence
	));

	return fence;
}

VkSemaphore Renderer::CreateSemaphore() const
{
	VkSemaphore semaphore = nullptr;
	VkSemaphoreCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = VK_SEMAPHORE_TYPE_BINARY;
	VK_CHECK(vkCreateSemaphore(
		mDevice,
		&createInfo,
		nullptr,
		&semaphore
	));

	return semaphore;
}

int32_t Renderer::FindMemoryIndex(uint32_t memoryTypeBits, VkMemoryPropertyFlags requestedMemoryType) const
{
	for (uint32_t i = 0; i < mPhysicalDevice.memoryProperties.memoryTypeCount; i++)
	{
		if (
			memoryTypeBits & (1 << i) &&
			(mPhysicalDevice.memoryProperties.memoryTypes[i].propertyFlags & requestedMemoryType) == requestedMemoryType
			)
		{
			return i;
		}
	}
	return -1;
}

Buffer Renderer::CreateBuffer(VkBufferUsageFlags bufferUsage, VkDeviceSize bufferSize, bool cpuAccessible) const
{
	Buffer buffer;
	VkBufferCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.size = bufferSize;
	createInfo.usage = bufferUsage;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;	// TODO: Only using with one queue.
	// queue families can be null if buffer sharingMode = VK_SHARING_MODE_EXCLUSE.
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;

	VK_CHECK(vkCreateBuffer(
		mDevice,
		&createInfo,
		nullptr,
		&buffer.buffer
	));

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(mDevice, buffer.buffer, &memoryRequirements);
	buffer.size = static_cast<uint32_t>(memoryRequirements.size);

	VkMemoryPropertyFlagBits memoryFlagBit = cpuAccessible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT :
															 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	int32_t memIndex = FindMemoryIndex(memoryRequirements.memoryTypeBits, memoryFlagBit);

	VkMemoryAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.allocationSize = static_cast<uint32_t>(memoryRequirements.size);
	allocateInfo.memoryTypeIndex = memIndex;

	VK_CHECK(vkAllocateMemory(
		mDevice,
		&allocateInfo,
		nullptr,
		&buffer.memory
	));

	return buffer;
}

Buffer Renderer::CreateUploadBuffer(VkDeviceSize bufferSize) const
{
	Buffer buffer;
	VkBufferCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.flags = 0;
	createInfo.size = bufferSize;
	createInfo.usage = VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;	// TODO: Only using with one queue.
	// queue families can be null if buffer sharingMode = VK_SHARING_MODE_EXCLUSE.
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;

	VK_CHECK(vkCreateBuffer(
		mDevice,
		&createInfo,
		nullptr,
		&buffer.buffer
	));

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(mDevice, buffer.buffer, &memoryRequirements);
	buffer.size = static_cast<uint32_t>(memoryRequirements.size);

	VkMemoryPropertyFlagBits memoryFlagBit = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

	int32_t memIndex = FindMemoryIndex(memoryRequirements.memoryTypeBits, memoryFlagBit);

	VkMemoryAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = memIndex;

	VK_CHECK(vkAllocateMemory(
		mDevice,
		&allocateInfo,
		nullptr,
		&buffer.memory
	));

	return buffer;
}

void Renderer::UploadToBuffer(Buffer& destinationBuffer, Buffer& uploadBuffer, const void* data, VkDeviceSize bufferSize)
{
	VK_CHECK(vkDeviceWaitIdle(mDevice));
	void* mappedData = nullptr;
	VK_CHECK(vkMapMemory(
		mDevice,
		uploadBuffer.memory,
		0,
		bufferSize,
		0,
		&mappedData
	));

	memcpy(mappedData, data, static_cast<size_t>(bufferSize));

	VkBufferCopy bufferCopy;
	bufferCopy.srcOffset = 0;
	bufferCopy.dstOffset = 0;
	bufferCopy.size = bufferSize;

	VK_CHECK(vkResetCommandPool(
		mDevice,
		mMainTransferCmdPool,
		0
	));

	VK_CHECK(vkResetCommandBuffer(
		mMainTransferCmd,
		0
	));

	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	VK_CHECK(vkBeginCommandBuffer(mMainTransferCmd, &beginInfo));

	vkCmdCopyBuffer(mMainTransferCmd, uploadBuffer.buffer, destinationBuffer.buffer, 1, &bufferCopy);
	
	VK_CHECK(vkEndCommandBuffer(mMainTransferCmd));

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &mMainTransferCmd;
	submitInfo.pWaitDstStageMask = nullptr;

	VK_CHECK(vkQueueSubmit(mTransferQueue, 1u, &submitInfo, nullptr));

	VK_CHECK(vkDeviceWaitIdle(mDevice));

	vkUnmapMemory(mDevice, uploadBuffer.memory);
}

void Renderer::BindBuffer(const Buffer& buffer, VkDeviceSize offset) const
{
	VK_CHECK(vkBindBufferMemory(
		mDevice,
		buffer.buffer,
		buffer.memory,
		offset
	));
}

Buffer Renderer::CreateUniformBuffer(uint64_t bufferSize) const
{
	Buffer buffer;
	VkBufferCreateInfo createInfo;
	createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	createInfo.pNext = nullptr;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;
	createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.size = CalculateUniformBufferSize(bufferSize);
	createInfo.flags = 0;
	createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	VK_CHECK(vkCreateBuffer(mDevice, &createInfo, nullptr, &buffer.buffer));

	VkMemoryRequirements requirements;
	vkGetBufferMemoryRequirements(mDevice, buffer.buffer, &requirements);
	buffer.size = static_cast<uint32_t>(requirements.size);

	VkMemoryAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.memoryTypeIndex = FindMemoryIndex(
		requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
	);
	allocateInfo.allocationSize = requirements.size;
	allocateInfo.pNext = nullptr;

	VK_CHECK(vkAllocateMemory(mDevice, &allocateInfo, nullptr, &buffer.memory));

	return buffer;
}

VkDescriptorSet Renderer::CreateDescriptorSet() const
{
	VkDescriptorSet descriptorSet = nullptr;
	VkDescriptorSetAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.descriptorPool = mGlobalDescriptorPool;
	allocateInfo.pSetLayouts = &mGlobalDescriptorSetLayout;

	VK_CHECK(vkAllocateDescriptorSets(mDevice, &allocateInfo, &descriptorSet));

	return descriptorSet;
}

MeshGeometry Renderer::CreateMeshGeometry()
{
	MeshGeometry meshGeometry{};
	
	VertexBufferModel vertexBuffer[] =
	{
		{ {  0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
		{ {  0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } },

		{ {  0.5f,  0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
		{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
	};

	uint32_t indexBuffer[] =
	{
		0, 1, 2,

		1, 2, 4, 1, 4, 3
	};

	meshGeometry.Geometries["Triangle"].indexCount = 3;
	meshGeometry.Geometries["Triangle"].firstIndex = 0;
	meshGeometry.Geometries["Triangle"].vertexOffset = 0;

	meshGeometry.Geometries["Square"].indexCount = 6;
	meshGeometry.Geometries["Square"].firstIndex = 3;
	meshGeometry.Geometries["Square"].vertexOffset = 0;

	meshGeometry.VertexBuffer = CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(vertexBuffer), false);
	BindBuffer(meshGeometry.VertexBuffer);
	
	Buffer upBuf = CreateUploadBuffer(sizeof(vertexBuffer));
	BindBuffer(upBuf);
	UploadToBuffer(meshGeometry.VertexBuffer, upBuf, vertexBuffer, upBuf.size);
	DestroyBuffer(&upBuf);
	
	meshGeometry.IndexBuffer = CreateBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(indexBuffer), false);
	BindBuffer(meshGeometry.IndexBuffer);

	upBuf = CreateUploadBuffer(sizeof(indexBuffer));
	BindBuffer(upBuf);
	UploadToBuffer(meshGeometry.IndexBuffer, upBuf, indexBuffer, upBuf.size);
	DestroyBuffer(&upBuf);

	return meshGeometry;
}

void Renderer::DestroyBuffer(Buffer* buffer) const
{
	vkFreeMemory(mDevice, buffer->memory, nullptr);
	vkDestroyBuffer(mDevice, buffer->buffer, nullptr);
	ZeroMemory(buffer, sizeof(*buffer));
}
