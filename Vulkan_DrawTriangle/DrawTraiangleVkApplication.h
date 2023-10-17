#pragma once

#include <filesystem>
#include <set>
#include <backends/imgui_impl_glfw.h>

#include "Camera.h"
#include "Image.h"
#include "Mesh.h"
#include "VkUtils.h"
#include "VkHelper.h"
#include "VkImGUI.h"
#include "Singleton.h"
#include "../Common/imgui/imgui.h"
#include "../Common/imgui/backends/imgui_impl_vulkan.h"

class DrawTriangleVkApplication
{

#pragma region Common

public:

	VkInstance GetVkInstance() { return vkInstance; }

	void InitVulkan(std::vector<const char*> extensions, GLFWwindow* window)
	{
		InitVkInstance(extensions);
		InitDebugMessenger();
		CreateSurface(window);
		PickPhysicalDevice();
		CreateLogicDevice();
		CreateSwapChain();

		CreateCommandPool();
		CreateCommandBuffer();

		CreateRenderPass();

		CreateImGUI();
		CreateCamera();

		CreateImageViews();
		CreateDepthResources();
		CreateFrameBuffers();

		CreateUniformBuffer();
		LoadSimpleGeometry();
		LoadImage();

		CreateGraphicsPipeline();

		CreateSyncObjects();
	}

	void CleanUp()
	{
		CheckVulkanResult(vkDeviceWaitIdle(vkDevice));

		mesh.reset();
		image.reset();
		MVPUniformBuffer.reset();
		imGUI.reset();

		CleanUpSwapChain();

		for (int i = 0; i < VkUtils::MaxFrameInFlight; i++)
		{
			vkDestroySemaphore(vkDevice, vkImageAvailableSemaphore[i], nullptr);
			vkDestroySemaphore(vkDevice, vkRenderFinishedSemaphore[i], nullptr);
			vkDestroyFence(vkDevice, vkInFlightFence[i], nullptr);
		}

		vkDestroyCommandPool(vkDevice, vkCommandPool, nullptr);

		vkDestroyPipeline(vkDevice, vkGraphicsPipeline, nullptr);
		vkDestroyPipelineLayout(vkDevice, vkPipelineLayout, nullptr);
		vkDestroyRenderPass(vkDevice, vkRenderPass, nullptr);

		vkDestroyDevice(vkDevice, nullptr);
#ifdef _DEBUG
		DestroyDebugMessenger();
#endif
		vkDestroySurfaceKHR(vkInstance, vkSurface, nullptr);
		vkDestroyInstance(vkInstance, nullptr);
	}

	void CreateSyncObjects()
	{
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		vkImageAvailableSemaphore.resize(VkUtils::MaxFrameInFlight);
		vkRenderFinishedSemaphore.resize(VkUtils::MaxFrameInFlight);
		vkInFlightFence.resize(VkUtils::MaxFrameInFlight);

		for (int i = 0; i < VkUtils::MaxFrameInFlight; i++)
		{
			CheckVulkanResult(vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &vkImageAvailableSemaphore[i]));
			CheckVulkanResult(vkCreateSemaphore(vkDevice, &semaphoreInfo, nullptr, &vkRenderFinishedSemaphore[i]));
			CheckVulkanResult(vkCreateFence(vkDevice, &fenceInfo, nullptr, &vkInFlightFence[i]));
		}
	}

	void Render()
	{
		// 等待上一帧执行完
		CheckVulkanResult(vkWaitForFences(vkDevice, 1, &vkInFlightFence[currentFrame], VK_TRUE, UINT64_MAX));

		// 从swapChain拿出可用的image Index
		uint32_t imageIndex;
		CheckVulkanResult(vkAcquireNextImageKHR(vkDevice, vkSwapChain, UINT64_MAX, vkImageAvailableSemaphore[currentFrame], VK_NULL_HANDLE, &imageIndex));

		// update uniform buffer
		VkUtils::MVP MVPMat;
		static auto startTime = std::chrono::high_resolution_clock::now();
		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		MVPMat.modelMat = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		MVPMat.viewMat = camera->matrices.view;
		MVPMat.projectiveMat = camera->matrices.perspective;
		MVPUniformBuffer->UpdateUniformBuffer(currentFrame, MVPMat);

		// Only reset the fence if we are submitting work
		CheckVulkanResult(vkResetFences(vkDevice, 1, &vkInFlightFence[currentFrame]));

		// 重置并记录Command
		CheckVulkanResult(vkResetCommandBuffer(vkCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0));

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		CheckVulkanResult(vkBeginCommandBuffer(vkCommandBuffers[currentFrame], &beginInfo));

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = vkRenderPass;
		renderPassInfo.framebuffer = vkSwapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChainExtent;

		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f,1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = clearValues.size();
		renderPassInfo.pClearValues = clearValues.data();

		imGUI->NewFrame();
		imGUI->UpdateBuffer();

		vkCmdBeginRenderPass(vkCommandBuffers[currentFrame], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		RecordCommandBuffer(vkCommandBuffers[currentFrame], imageIndex);

		imGUI->DrawFrame(vkCommandBuffers[currentFrame]);

		vkCmdEndRenderPass(vkCommandBuffers[currentFrame]);

		CheckVulkanResult(vkEndCommandBuffer(vkCommandBuffers[currentFrame]));

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		// We want to wait with writing colors to the image until it's available,
		// so we're specifying the stage of the graphics pipeline that writes to the color attachment.
		// That means that theoretically the implementation can already start executing
		// our vertex shader and such while the image is not yet available.
		//我们新提交 CommandBuffer 中的 command 并不会阻塞，而是会开始执行，
		//但是执行到 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 阶段就会被阻塞住，
		//直到 semaphore 被 signaled
		VkSemaphore waitSemaphores[] = { vkImageAvailableSemaphore[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &vkCommandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { vkRenderFinishedSemaphore[currentFrame] };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		// 往Queue里提交命令
		CheckVulkanResult(vkQueueSubmit(vkGraphicsQueue, 1, &submitInfo, vkInFlightFence[currentFrame]));

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { vkSwapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		CheckVulkanResult(vkQueuePresentKHR(vkPresentQueue, &presentInfo));
		CheckVulkanResult(vkQueueWaitIdle(vkGraphicsQueue));

		currentFrame = (currentFrame + 1) % VkUtils::MaxFrameInFlight;
	}

	void Run()
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		Render();
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		float frameTimer = (float)tDiff / 1000.0f;
		camera->update(frameTimer);
	}

	void InitVkInstance(std::vector<const char*> instanceExtensions)
	{
#ifdef _DEBUG
		if (!CheckValidationLayerSupport())
			throw std::runtime_error("validation layers requested, but not available!");
#endif

		// application information
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Draw Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		// Create vkInstance
		{
			VkInstanceCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

			uint32_t propertiesCount = 0;
			std::vector<VkExtensionProperties> properties;
			CheckVulkanResult(vkEnumerateInstanceExtensionProperties(nullptr, &propertiesCount, nullptr));
			properties.resize(propertiesCount);
			CheckVulkanResult(vkEnumerateInstanceExtensionProperties(nullptr, &propertiesCount, properties.data()));

			if (VkHelper::IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
				instanceExtensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
			if (VkHelper::IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
			{
				instanceExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
				createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
			}
#endif

			createInfo.pApplicationInfo = &appInfo;
			createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
			createInfo.ppEnabledExtensionNames = instanceExtensions.data();

#ifdef _DEBUG
			VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
			createInfo.enabledLayerCount = VkUtils::DefaultValidationLayersCount;
			createInfo.ppEnabledLayerNames = VkUtils::DefaultValidationLayers.data();
			VkUtils::PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
#endif

			CheckVulkanResult(vkCreateInstance(&createInfo, nullptr, &vkInstance));
		}
	}

	void LoadSimpleGeometry()
	{
		mesh = std::make_unique<Geometry::Mesh>(vkPhysicalDevice, vkDevice,
			vkCommandPool, vkGraphicsQueue);
		// mesh->LoadTexturedRectangle();
		mesh->LoadOverlappingTexturedRectangle();
		// mesh->LoadSimpleRectangleWithoutUV();
	}

	void LoadImage()
	{
		image = std::make_unique<Image>(vkPhysicalDevice, vkDevice,
			vkCommandPool, vkGraphicsQueue);
		image->LoadDefaultImage();
	}

private:
	VkInstance vkInstance;
	VkPhysicalDevice vkPhysicalDevice;
	VkDevice vkDevice;
	VkSurfaceKHR vkSurface;

	// vulkan queue
	VkQueue vkGraphicsQueue;
	VkQueue vkPresentQueue;

	uint32_t currentFrame = 0;
	std::vector<VkSemaphore> vkImageAvailableSemaphore;
	std::vector<VkSemaphore> vkRenderFinishedSemaphore;
	std::vector<VkFence> vkInFlightFence;

	std::unique_ptr<Geometry::Mesh> mesh;
	std::unique_ptr<Image> image;

#pragma endregion Common

#pragma region Debug
public:
	void InitDebugMessenger()
	{
#ifdef _DEBUG
		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		VkUtils::PopulateDebugMessengerCreateInfo(createInfo);

		if (VkUtils::CreateDebugUtilsMessengerEXT(vkInstance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug messenger!");
		}
#endif
	}

	bool CheckValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : VkUtils::DefaultValidationLayers) {
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers) {
				if (strcmp(layerName, layerProperties.layerName) == 0) {
					layerFound = true;
					break;
				}
			}

			if (!layerFound) {
				return false;
			}
		}

		return true;
	}

private:

	void DestroyDebugMessenger()
	{
#ifdef _DEBUG
		VkUtils::DestroyDebugUtilsMessengerEXT(vkInstance, debugMessenger, nullptr);
#endif
	}

private:
#ifdef _DEBUG
	VkDebugUtilsMessengerEXT debugMessenger;
#endif

#pragma endregion Debug

#pragma region PickPhysicalDevice

public:
	void PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		CheckVulkanResult(vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr));
		if (deviceCount == 0)
			throw std::runtime_error("failed to find GPUs with Vulkan support!");

		std::vector<VkPhysicalDevice> devices(deviceCount);
		CheckVulkanResult(vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data()));

		for(const auto&device:devices)
		{
			if (IsDeviceSuitable(device))
			{
				vkPhysicalDevice = device;
				break;
			}
		}

		if (vkPhysicalDevice == VK_NULL_HANDLE)
			throw std::runtime_error("failed to find a suitable GPU!");
	}

private:
	bool IsDeviceSuitable(VkPhysicalDevice device)
	{
		VkUtils::QueueFamilyIndices indices = VkUtils::FindQueueFamilies(device, vkSurface);
		std::set<std::string> requiredExtensions(VkUtils::DefaultDeviceExtensions.begin(), VkUtils::DefaultDeviceExtensions.end());
		bool extensionsSupported = VkUtils::CheckDeviceExtensionSupport(device, requiredExtensions);
		bool swapChainAdequate = false;
		if(extensionsSupported)
		{
			VkUtils::SwapChainSupportDetails swapChainSupportDetails = QuerySwapChainSupport(device);
			swapChainAdequate = !swapChainSupportDetails.formats.empty()
			&& !swapChainSupportDetails.presentModes.empty();
		}

		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return indices.isComplete() && extensionsSupported && swapChainAdequate
		&& supportedFeatures.samplerAnisotropy;
	}

#pragma endregion PickPhysicalDevice

#pragma region CreateLogicDevice

public:
	void CreateLogicDevice()
	{
		VkUtils::QueueFamilyIndices queueFamilyIndices = VkUtils::FindQueueFamilies(vkPhysicalDevice, vkSurface);
		if (!queueFamilyIndices.isComplete())
			throw std::runtime_error("Failed to find appropriate queue in device");

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies =
		{ queueFamilyIndices.graphicsFamily.value(),queueFamilyIndices.presentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamilyIndex :uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo deviceCreateInfo{};
		deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

		deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

		// device extension
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(VkUtils::DefaultDeviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = VkUtils::DefaultDeviceExtensions.data();

#ifdef _DEBUG
		deviceCreateInfo.enabledLayerCount = VkUtils::DefaultValidationLayersCount;
		deviceCreateInfo.ppEnabledLayerNames = VkUtils::DefaultValidationLayers.data();
#else
		deviceCreateInfo.enabledLayerCount = 0;
#endif

		CheckVulkanResult(vkCreateDevice(vkPhysicalDevice, &deviceCreateInfo, nullptr, &vkDevice));

		vkGetDeviceQueue(vkDevice, queueFamilyIndices.graphicsFamily.value(), 0, &vkGraphicsQueue);
		vkGetDeviceQueue(vkDevice, queueFamilyIndices.presentFamily.value(), 0, &vkPresentQueue);
	}

#pragma endregion CreateLogicDevice

#pragma region SwapChain

public:
	void CreateSwapChain()
	{
		VkUtils::SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(vkPhysicalDevice);

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		imageCount = std::clamp(imageCount, swapChainSupport.capabilities.minImageCount,
			swapChainSupport.capabilities.maxImageCount);

		VkSwapchainCreateInfoKHR swapChainCreateInfo{};
		swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapChainCreateInfo.surface = vkSurface;
		swapChainCreateInfo.minImageCount = imageCount;
		swapChainCreateInfo.imageFormat = surfaceFormat.format;
		swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
		swapChainCreateInfo.imageExtent = extent;
		swapChainCreateInfo.imageArrayLayers = 1;
		swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		VkUtils::QueueFamilyIndices indices = VkUtils::FindQueueFamilies(vkPhysicalDevice, vkSurface);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		if (indices.graphicsFamily != indices.presentFamily) {
			swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			swapChainCreateInfo.queueFamilyIndexCount = 2;
			swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapChainCreateInfo.presentMode = presentMode;
		swapChainCreateInfo.clipped = VK_TRUE;

		swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

		CheckVulkanResult(vkCreateSwapchainKHR(vkDevice, &swapChainCreateInfo, nullptr, &vkSwapChain));

		CheckVulkanResult(vkGetSwapchainImagesKHR(vkDevice, vkSwapChain, &imageCount, nullptr));
		swapChainImages.resize(imageCount);
		CheckVulkanResult(vkGetSwapchainImagesKHR(vkDevice, vkSwapChain, &imageCount, swapChainImages.data()));

		// cache for other module in program
		swapChainPresentMode = presentMode;
		swapChainMinImageCount = imageCount;
	}

	void ReCreateVulkanResource()
	{
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(vkDevice);

		CleanUpSwapChain();

		CreateSwapChain();
		CreateImageViews();
		CreateDepthResources();
		CreateFrameBuffers();
	}

private:

	void CleanUpSwapChain()
	{
		vkDestroyImageView(vkDevice, vkDepthImageView, nullptr);
		vkDestroyImage(vkDevice, vkDepthImage, nullptr);
		vkFreeMemory(vkDevice, vkDepthImageMemory, nullptr);

		for (size_t i = 0; i < vkSwapChainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(vkDevice, vkSwapChainFramebuffers[i], nullptr);
		}

		for (size_t i = 0; i < vkSwapChainImageViews.size(); i++) {
			vkDestroyImageView(vkDevice, vkSwapChainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(vkDevice, vkSwapChain, nullptr);
	}

	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		for (const auto& availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}

		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
			return capabilities.currentExtent;
		}
		else {
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	VkUtils::SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device)
	{
		VkUtils::SwapChainSupportDetails swapChainSupportDetails;

		// surface capability
		CheckVulkanResult(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, vkSurface, &swapChainSupportDetails.capabilities));

		// surface format
		uint32_t formatCount = 0;
		CheckVulkanResult(vkGetPhysicalDeviceSurfaceFormatsKHR(device, vkSurface, &formatCount, nullptr));
		if (formatCount != 0)
		{
			swapChainSupportDetails.formats.resize(formatCount);
			CheckVulkanResult(vkGetPhysicalDeviceSurfaceFormatsKHR(device, vkSurface, &formatCount, swapChainSupportDetails.formats.data()));
		}

		// present mode
		uint32_t presentModeCount;
		CheckVulkanResult(vkGetPhysicalDeviceSurfacePresentModesKHR(device, vkSurface, &presentModeCount, nullptr));
		if (presentModeCount != 0) {
			swapChainSupportDetails.presentModes.resize(presentModeCount);
			CheckVulkanResult(vkGetPhysicalDeviceSurfacePresentModesKHR(device, vkSurface, &presentModeCount, swapChainSupportDetails.presentModes.data()));
		}

		return swapChainSupportDetails;
	}
private:
	// swap chain
	uint32_t swapChainMinImageCount;
	VkSwapchainKHR vkSwapChain;
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat;
	VkPresentModeKHR swapChainPresentMode;
	VkExtent2D swapChainExtent;

#pragma endregion SwapChain

#pragma region CreateImageView

public:
	void CreateImageViews()
	{
		vkSwapChainImageViews.resize(swapChainImages.size());
		for (int i = 0; i < swapChainImages.size(); i++)
			vkSwapChainImageViews[i] = VkUtils::CreateImageView(vkDevice, swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}

private:
	std::vector<VkImageView> vkSwapChainImageViews;

#pragma endregion CreateImageView

#pragma region DepthImage

private:
	void CreateDepthResources()
	{
		VkFormat depthFormat = FindDepthFormat();

		VkUtils::CreateImage(vkPhysicalDevice,vkDevice, swapChainExtent.width, swapChainExtent.height, 
			depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vkDepthImage, vkDepthImageMemory);
		vkDepthImageView = VkUtils::CreateImageView(vkDevice, vkDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

		VkUtils::TransitionImageLayout(vkDevice, vkCommandPool, vkGraphicsQueue, vkDepthImage, depthFormat,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
	}

	VkFormat FindDepthFormat()
	{
		return VkUtils::FindSupportedFormat(vkPhysicalDevice,
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

private:
	VkImage vkDepthImage;
	VkDeviceMemory vkDepthImageMemory;
	VkImageView vkDepthImageView;

#pragma endregion DepthImage

#pragma region RenderPass

public:
	// During normal rendering, it is not possible for a fragment shader to access the attachments to which it is currently rendering: GPUs have optimized hardware for writing to the attachments, and accessing the attachment interferes with this.
	// However, some common rendering techniques such as deferred shading rely on being able to access the result of previous rendering during shading. For a tile-based renderer, the results of previous rendering can efficiently stay on-chip if subsequent rendering operations are at the same resolution,
	// and if only the data in the pixel currently being rendered is needed (accessing different pixels may require access to values outside the current tile, which breaks this optimization).
	// In order to help optimize deferred shading on tile-based renderers, Vulkan splits the rendering operations of a render pass into subpasses.
	// All subpasses in a render pass share the same resolution and tile arrangement, and as a result, they can access the results of previous subpass.
	void CreateRenderPass()
	{
		// attachment is as input and output of render pass
		// the below is just attachment description!!
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		// depth attachment
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = FindDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		CheckVulkanResult(vkCreateRenderPass(vkDevice, &renderPassInfo, nullptr, &vkRenderPass));
	}

private:
	VkRenderPass vkRenderPass;

#pragma endregion RenderPass

#pragma region UniformBuffer

private:
	void CreateUniformBuffer()
	{
		MVPUniformBuffer = 
			std::make_unique<VkUtils::UniformBuffer<VkUtils::MVP>>(vkPhysicalDevice, vkDevice, 
				VkUtils::MaxFrameInFlight);
	}

private:
	std::unique_ptr<VkUtils::UniformBuffer<VkUtils::MVP>> MVPUniformBuffer;

#pragma endregion UniformBuffer

#pragma region CreateGraphicsPipeline
public:

	void CreateGraphicsPipeline()
	{
		std::string workingPath = std::filesystem::current_path().string();
		std::string vertShader = workingPath + "/Shaders/drawTriangleVertShader.spv";
		std::string fragShader = workingPath + "/Shaders/drawTriangleFragShader.spv";

		// load shader code
		auto vertShaderCode = VkUtils::ReadBinaryFile(vertShader);
		auto fragShaderCode = VkUtils::ReadBinaryFile(fragShader);

		// create shader module
		VkShaderModule vertShaderModule = VkUtils::CreateShaderModule(vkDevice, vertShaderCode);
		VkShaderModule fragShaderModule = VkUtils::CreateShaderModule(vkDevice, fragShaderCode);

		// create shader stage
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		// vertex input (bind and attribute)
		auto bindingDescription = Geometry::Vertex::GetBindingDescription();
		auto attributeDescriptions = Geometry::Vertex::GetAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = 
			static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

		// primitive topology
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		// view port
		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		// rasterization
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		// multi-sampling
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// color-blending
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;

		// depth & stencil state
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;
		depthStencil.depthWriteEnable = VK_TRUE;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional

		// config pipeline state
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		// pipeline layout (uniform value/global value in shader,like MVP matrix .etc)
		// std::vector<VkDescriptorSetLayout> descriptorSets{ MVPUniformBuffer->GetDescriptorSetLayout()};
		std::vector<VkDescriptorSetLayout> descriptorSets { MVPUniformBuffer->GetDescriptorSetLayout(),
			image->GetTextureSampler()->GetDescriptorSetLayout()};
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSets.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSets.data();
		pipelineLayoutInfo.pushConstantRangeCount = 0;

		CheckVulkanResult(vkCreatePipelineLayout(vkDevice, &pipelineLayoutInfo, nullptr, &vkPipelineLayout));

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = vkPipelineLayout;
		pipelineInfo.renderPass = vkRenderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.pDepthStencilState = &depthStencil;

		CheckVulkanResult(vkCreateGraphicsPipelines(vkDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vkGraphicsPipeline));

		vkDestroyShaderModule(vkDevice, fragShaderModule, nullptr);
		vkDestroyShaderModule(vkDevice, vertShaderModule, nullptr);
	}

private:
	VkPipeline vkGraphicsPipeline;
	VkPipelineLayout vkPipelineLayout;

#pragma endregion CreateGraphicsPipeline

#pragma region FrameBuffer

public:
	// Framebuffers represent a collection of memory attachments that are used by a render pass instance.
	// A framebuffer provides the attachments that a render pass needs while rendering.
	// The framebuffer essentially associates the actual attachments to the renderpass.
	void CreateFrameBuffers()
	{
		vkSwapChainFramebuffers.resize(vkSwapChainImageViews.size());
		for (size_t i = 0; i < vkSwapChainImageViews.size(); i++) {
			std::array<VkImageView, 2> attachments = { vkSwapChainImageViews[i],vkDepthImageView };

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = vkRenderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			CheckVulkanResult(vkCreateFramebuffer(vkDevice, &framebufferInfo, nullptr, &vkSwapChainFramebuffers[i]));
		}
	}

private:
	std::vector<VkFramebuffer> vkSwapChainFramebuffers;

#pragma endregion FrameBuffer

#pragma region Command

public:
	void CreateCommandPool()
	{
		VkUtils::QueueFamilyIndices queueFamilyIndices = VkUtils::FindQueueFamilies(vkPhysicalDevice, vkSurface);

		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

		CheckVulkanResult(vkCreateCommandPool(vkDevice, &poolInfo, nullptr, &vkCommandPool));
	}

	void CreateCommandBuffer()
	{
		vkCommandBuffers.resize(VkUtils::MaxFrameInFlight);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = vkCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(vkCommandBuffers.size());

		CheckVulkanResult(vkAllocateCommandBuffers(vkDevice, &allocInfo, vkCommandBuffers.data()));
	}

	void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkGraphicsPipeline);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkBuffer vertexBuffers[] = { mesh->GetVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(commandBuffer, mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout,
			0, 1, MVPUniformBuffer->GetAddressOfDescriptorSet(currentFrame),
			0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout,
			1, 1, image->GetTextureSampler()->GetAddressOfDescriptorSet(currentFrame),
			0, nullptr);

		vkCmdDrawIndexed(commandBuffer, mesh->IndexNumber(), 1, 0, 0, 0);
	}

	// void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	// {
	// 	VkCommandBufferBeginInfo beginInfo{};
	// 	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	// 	beginInfo.flags = 0; // Optional
	// 	beginInfo.pInheritanceInfo = nullptr; // Optional
	//
	// 	CheckVulkanResult(vkBeginCommandBuffer(commandBuffer, &beginInfo));
	//
	// 	VkRenderPassBeginInfo renderPassInfo{};
	// 	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	// 	renderPassInfo.renderPass = vkRenderPass;
	// 	renderPassInfo.framebuffer = vkSwapChainFramebuffers[imageIndex];
	// 	renderPassInfo.renderArea.offset = { 0, 0 };
	// 	renderPassInfo.renderArea.extent = swapChainExtent;
	//
	// 	std::array<VkClearValue, 2> clearValues{};
	// 	clearValues[0].color = { 0.0f, 0.0f, 0.0f,1.0f };
	// 	clearValues[1].depthStencil = { 1.0f, 0 };
	//
	// 	renderPassInfo.clearValueCount = clearValues.size();
	// 	renderPassInfo.pClearValues = clearValues.data();
	//
	// 	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	//
	// 	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkGraphicsPipeline);
	//
	// 	VkViewport viewport{};
	// 	viewport.x = 0.0f;
	// 	viewport.y = 0.0f;
	// 	viewport.width = (float)swapChainExtent.width;
	// 	viewport.height = (float)swapChainExtent.height;
	// 	viewport.minDepth = 0.0f;
	// 	viewport.maxDepth = 1.0f;
	// 	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	// 	
	// 	VkRect2D scissor{};
	// 	scissor.offset = { 0, 0 };
	// 	scissor.extent = swapChainExtent;
	// 	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	//
	// 	VkBuffer vertexBuffers[] = { mesh->GetVertexBuffer() };
	// 	VkDeviceSize offsets[] = { 0 };
	// 	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
	// 	vkCmdBindIndexBuffer(commandBuffer, mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
	// 	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout,
	// 		0, 1, MVPUniformBuffer->GetAddressOfDescriptorSet(currentFrame),
	// 		0, nullptr);
	// 	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout,
	// 		1, 1, image->GetTextureSampler()->GetAddressOfDescriptorSet(currentFrame),
	// 		0, nullptr);
	//
	// 	vkCmdDrawIndexed(commandBuffer, mesh->IndexNumber(), 1, 0, 0, 0);
	// 	vkCmdEndRenderPass(commandBuffer);
	//
	// 	CheckVulkanResult(vkEndCommandBuffer(commandBuffer));
	// }

private:
	// commandPool equal to commandAllocator in dx12
	VkCommandPool vkCommandPool;
	// commandBuffer equal to commandList in dx12
	std::vector<VkCommandBuffer> vkCommandBuffers;

#pragma endregion Command

#pragma region GLFW

public:
	void CreateSurface(GLFWwindow* window)
	{
		this->window = window;
		CheckVulkanResult(glfwCreateWindowSurface(vkInstance, window, nullptr, &vkSurface));
	}

private:
	GLFWwindow* window;

#pragma endregion GLFW

#pragma region ImGUI

private:
	void CreateImGUI()
	{
		ImGUICreateInfo createInfo;

		createInfo.instance = vkInstance;
		createInfo.commandPool = vkCommandPool;

		createInfo.physicalDevice = vkPhysicalDevice;
		createInfo.logicalDevice = vkDevice;
		createInfo.surface = vkSurface;

		createInfo.swapChain = vkSwapChain;
		createInfo.swapChainImageFormat = swapChainImageFormat;
		createInfo.swapChainPresentMode = swapChainPresentMode;
		createInfo.swapChainImageWidth = swapChainExtent.width;
		createInfo.swapChainImageHeight = swapChainExtent.height;
		createInfo.swapChainImageCount = swapChainMinImageCount;

		createInfo.renderPass = vkRenderPass;
		createInfo.graphicsQueue = vkGraphicsQueue;
		createInfo.glfwWindow = window;

		imGUI = std::make_unique<VkImGUI>(createInfo);
	}
private:
	std::unique_ptr<VkImGUI> imGUI;

#pragma endregion ImGUI

#pragma region Camera
private:
	Camera* camera = nullptr;
private:
	void CreateCamera()
	{
		camera = Singleton<Camera>::Instance();
		camera->flipY = true;
		camera->type = Camera::CameraType::lookat;
		
		camera->setPosition(glm::vec3(2.0f, 2.0f, 2.0f));
		camera->setTarget(glm::vec3(0.0f, 0.0f, 0.0f));
		camera->setPerspective(45.0f, swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
	}

#pragma endregion Camera
};
