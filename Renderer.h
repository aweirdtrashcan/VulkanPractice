#pragma once

#include "HelperStructs.h"
#include <cassert>
#include "EngineException.h"
#include "Timer.h"
#include <DirectXMath.h>
#include "MeshGeometry.h"
#include "RenderItem.h"
#include "GeometryGenerator.h"

#define VK_CHECK(expr) { if ((expr)) { throw EngineException(__FILE__, __LINE__, #expr); } }

class Window;

class Renderer
{
public:
	Renderer(const Window* window);
	~Renderer();
	
	void NotifyWindowResize(int width, int height);
	void ToggleVSync() {};

	void Update();
	void Draw();

	void OnKeyUp(int key);
	void OnKeyDown(int key);

	void OnMouseMove(uint64_t wParam, int x, int y);

	void WaitForDeviceIdle()
	{
		vkDeviceWaitIdle(mDevice);
	}

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
	Image CreateDepthBuffer() const;
	VkImageView CreateImageView(VkFormat viewFormat, VkImage image, VkImageAspectFlags imageAspect) const;
	VkShaderModule CreateShaderModule(const char* shaderPath) const;
	VkRenderPass CreateRenderPass() const;
	VkFramebuffer CreateFramebuffer(VkRenderPass renderpass, uint32_t numImageViews, VkImageView* imageViews, uint32_t width, uint32_t height) const;
	Buffer CreateGlobalUniformBuffer(uint32_t numFrames) const;
	VkDescriptorSetLayout CreateDescriptorSetLayout() const;
	VkDescriptorPool CreateDescriptorPool() const;
	std::vector<VkDescriptorSet> AllocateGlobalDescriptorSets() const;
	void UpdateUniformBuffer(Buffer buffer, uint64_t bufferStride, uint64_t offset, void* data) const;
	void UpdateDescriptorSet(Buffer buffer, uint64_t bufferStride, VkDescriptorSet descriptorSet, uint64_t offset, uint32_t binding) const;
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
	Buffer CreateUniformBuffer(uint64_t bufferSize) const;
	VkDescriptorSet CreateDescriptorSet() const;
	MeshGeometry CreateMeshGeometry();
	void UpdateGlobalUniformData(GlobalUniform& globalUniform) const;
	void CalculateDeltaTime();
	MeshGeometry BuildLandGeometry();
	void BuildShapesRenderItems();
	void BuildLandRenderItems();
	float GetHillsHeight(float x, float z) const
	{
		return 0.3f * (z * sinf(0.1f * x) + (x * cosf(0.1f * z)));
	}

private:
	static constexpr int shaderCodeMaxSize = 1024 * 10;
	Timer mTimer;

	bool mResizing = false;

	double mAccumulatedDelta = 0.0;
	int mFps = 0;
	float mDeltaTime = 0.0f;

	const Window* mWindow;
	VkInstance mInstance = nullptr;
	VkDebugUtilsMessengerEXT mDebugMessenger = nullptr;
	
	VkPhysicalDeviceInfo mPhysicalDevice = {};
	VkDevice mDevice = nullptr;
	VkDeviceInfo mDeviceInfo;
	uint32_t mGraphicsQueueIndex = 0;
	
	VkQueue mGraphicsQueue = nullptr;
	VkQueue mTransferQueue = nullptr;
	
	VkCommandBuffer mMainCmd = nullptr;
	VkCommandPool mMainCmdPool = nullptr;
	VkCommandBuffer mMainTransferCmd = nullptr;
	VkCommandPool mMainTransferCmdPool = nullptr;
	
	VkFence mMainCopyFence = nullptr;
	VkSemaphore mMainCopyDoneSemaphore = nullptr;

	uint32_t mNextImageIndex = 0;
	uint32_t mCurrentImageIndex = 0;

	Image mDepthBuffer{};

	VkSurfaceKHR mSurface = nullptr;
	VkSwapchainKHR mSwapchain = nullptr;
	VkSurfaceFormatKHR mSwapchainSurfaceFormat = {};

	std::vector<VkImage> mImages;
	std::vector<VkImageView> mImageViews;
	uint32_t mImageCount = 0;

	std::vector<FrameResources> mFrameResources;

	VkRenderPass mRenderpass = nullptr;

	VkViewport mViewport = {};
	VkRect2D mScissor = {};

	VkDescriptorPool mGlobalDescriptorPool = nullptr;
	VkDescriptorSetLayout mGlobalDescriptorSetLayout = nullptr;
	GlobalUniform mGlobalUniform{};
	Buffer mGlobalUniformBuffer;
	Buffer mObjectUniformBuffer;

	VkPipelineLayout mPipelineLayout = nullptr;
	VkPipeline mGraphicsPipeline = nullptr;

	MeshGeometry mMeshGeometry;
	std::vector<RenderItem> mRenderItems;

	DirectX::XMVECTOR mEyePosition;
	struct
	{
		int x;
		int y;
	} mLastMousePos;
	float mTheta = 0.0f;
	float mPhi = 0.0f;
	float mRadius = 10.f;
	VkFence fence;
};
