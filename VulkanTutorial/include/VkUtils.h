#pragma once
#include <fstream>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <optional>

#include "VkHelper.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// constant
namespace VkUtils
{
	const uint32_t MaxFrameInFlight = 3;
	const std::vector<const char*> DefaultDeviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef _DEBUG
	const std::vector<const char*> DefaultDebugExtensions{ VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

	const std::vector<const char*> DefaultValidationLayers = { "VK_LAYER_KHRONOS_validation" };
	const int32_t DefaultValidationLayersCount = static_cast<int32_t>(DefaultValidationLayers.size());
#endif
}

// debug
namespace VkUtils
{
#ifdef _DEBUG

	static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
		return VK_FALSE;
	}

	inline void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = VulkanDebugCallback;
	}

	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr) {
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else {
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, pAllocator);
		}
	}

#endif
}

// data structure
namespace VkUtils
{
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;

		bool isComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};
}

// context
namespace VkUtils
{
	inline VkCommandBuffer BeginSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool)
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		CheckVulkanResult(vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer));

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		CheckVulkanResult(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		return commandBuffer;
	}

	inline void EndSingleTimeCommands(VkDevice logicalDevice, VkCommandPool commandPool,
		VkCommandBuffer commandBuffer, VkQueue graphicsQueue)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
	}
}

// vulkan memory
namespace VkUtils
{
	inline uint32_t FindAppropriateMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) &&
				(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}

	inline void CreateBuffer(VkPhysicalDevice physicalDevice,
		VkDevice logicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		CheckVulkanResult(vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer));

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindAppropriateMemoryType(physicalDevice,
			memRequirements.memoryTypeBits, properties);

		CheckVulkanResult(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory));
		CheckVulkanResult(vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0));
	}

	inline void CopyBuffer(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
		VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(logicalDevice, commandPool);

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		EndSingleTimeCommands(logicalDevice, commandPool, commandBuffer, graphicsQueue);
	}
}

// shader
namespace VkUtils
{
	inline const std::vector<char> ReadBinaryFile(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		
		file.close();
		return buffer;
	}

	struct MVP
	{
		alignas(16) glm::mat4 modelMat;
		alignas(16) glm::mat4 viewMat;
		alignas(16) glm::mat4 projectiveMat;
	};

	template<typename T>
	class UniformBuffer
	{
	public:

		UniformBuffer() = delete;
		UniformBuffer(VkPhysicalDevice physicalDeviceIn, VkDevice logicalDeviceIn, uint32_t maxFrameInFlightIn):
		physicalDevice(physicalDeviceIn),logicalDevice(logicalDeviceIn), maxFrameInFlight(maxFrameInFlightIn)
		{
			CreateDescriptorSetLayout();
			CreateUniformBuffer();
			CreateDescriptorPool();
			CreateDescriptorSet();
		}

		~UniformBuffer()
		{
			for (size_t i = 0; i < maxFrameInFlight; i++) {
				vkDestroyBuffer(logicalDevice, uniformBuffers[i], nullptr);
				vkFreeMemory(logicalDevice, uniformBuffersMemory[i], nullptr);
			}

			vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
			vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
		}

		VkDescriptorSetLayout GetDescriptorSetLayout() { return descriptorSetLayout; }

		VkDescriptorSetLayout* GetAddressOfDescriptorSetLayout() { return &descriptorSetLayout; }

		VkBuffer GetUniformBuffer(uint32_t currentFrame) { return uniformBuffers[currentFrame]; }

		VkDescriptorSet* GetAddressOfDescriptorSet(uint32_t currentFrame)
		{
			return &descriptorSets[currentFrame];
		}

		VkDescriptorSet GetDescriptorSet(uint32_t currentFrame)
		{
			return descriptorSets[currentFrame];
		}

		void UpdateUniformBuffer(uint32_t currentFrame, T& uniformObject)
		{
			memcpy(uniformBuffersMapped[currentFrame], &uniformObject, sizeof(T));
		}

	private:

		// A descriptor set is called a "set" because it can refer to an array of homogenous resources
		// that can be described with the same layout binding.
		void CreateDescriptorSetLayout()
		{
			VkDescriptorSetLayoutBinding uboLayoutBinding{};
			uboLayoutBinding.binding = 0;
			uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uboLayoutBinding.descriptorCount = 1;
			uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &uboLayoutBinding;
			CheckVulkanResult(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout));
		}

		// uniform buffer is GPU buffer, we use uniformBuffersMapped to transfer CPU data to GPU
		void CreateUniformBuffer()
		{
			VkDeviceSize bufferSize = sizeof(T);

			uniformBuffers.resize(maxFrameInFlight);
			uniformBuffersMemory.resize(maxFrameInFlight);
			uniformBuffersMapped.resize(maxFrameInFlight);

			for (size_t i = 0; i < maxFrameInFlight; i++) 
			{
				VkUtils::CreateBuffer(physicalDevice, logicalDevice, bufferSize,
					VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					uniformBuffers[i], uniformBuffersMemory[i]);

				CheckVulkanResult(vkMapMemory(logicalDevice, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]));
			}
		}

		// descriptor pool is similar with allocator in dx12
		void CreateDescriptorPool()
		{
			VkDescriptorPoolSize poolSize;
			poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			poolSize.descriptorCount = maxFrameInFlight;

			VkDescriptorPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.poolSizeCount = 1;
			poolInfo.pPoolSizes = &poolSize;
			poolInfo.maxSets = maxFrameInFlight;

			CheckVulkanResult(vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool));
		}

		void CreateDescriptorSet()
		{
			// use layout to crate descriptor set
			std::vector<VkDescriptorSetLayout> layouts(maxFrameInFlight, descriptorSetLayout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = maxFrameInFlight;
			allocInfo.pSetLayouts = layouts.data();

			descriptorSets.resize(maxFrameInFlight);
			CheckVulkanResult(vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()));

			for (size_t i = 0; i < maxFrameInFlight; i++) 
			{
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = uniformBuffers[i];
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(T);

				VkWriteDescriptorSet descriptorWrite{};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = descriptorSets[i];
				descriptorWrite.dstBinding = 0;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pBufferInfo = &bufferInfo;

				// bind descriptor set and uniform buffer
				vkUpdateDescriptorSets(logicalDevice, 1, &descriptorWrite, 0, nullptr);
			}
		}

	private:
		uint32_t maxFrameInFlight;
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
		VkDescriptorSetLayout descriptorSetLayout;
		std::vector<VkBuffer> uniformBuffers;
		std::vector<VkDeviceMemory> uniformBuffersMemory;
		std::vector<void*> uniformBuffersMapped;
		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> descriptorSets;
	};

	class Sampler
	{
	public:
		Sampler() = delete;
		Sampler(VkImageView textureImageViewIn, VkSampler rawTextureSampler, VkPhysicalDevice physicalDeviceIn, VkDevice logicalDeviceIn, uint32_t maxFrameInFlightIn) :
			 textureImageView(textureImageViewIn), textureSampler(rawTextureSampler), physicalDevice(physicalDeviceIn), logicalDevice(logicalDeviceIn), maxFrameInFlight(maxFrameInFlightIn)
		{
			CreateDescriptorSetLayout();
			CreateDescriptorPool();
			CreateDescriptorSet();
		}
		~Sampler()
		{
			vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
			vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
		}

		VkDescriptorSetLayout GetDescriptorSetLayout() { return descriptorSetLayout; }

		VkDescriptorSetLayout* GetAddressOfDescriptorSetLayout() { return &descriptorSetLayout; }

		VkDescriptorSet* GetAddressOfDescriptorSet(uint32_t currentFrame)
		{
			return &descriptorSets[currentFrame];
		}

		VkDescriptorSet GetDescriptorSet(uint32_t currentFrame)
		{
			return descriptorSets[currentFrame];
		}

	private:
		// A descriptor set is called a "set" because it can refer to an array of homogenous resources
		// that can be described with the same layout binding.
		void CreateDescriptorSetLayout()
		{
			VkDescriptorSetLayoutBinding samplerLayoutBinding{};
			samplerLayoutBinding.binding = 0;
			samplerLayoutBinding.descriptorCount = 1;
			samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			samplerLayoutBinding.pImmutableSamplers = nullptr;
			samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = 1;
			layoutInfo.pBindings = &samplerLayoutBinding;
			CheckVulkanResult(vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout));
		}

		// descriptor pool is similar with allocator in dx12
		void CreateDescriptorPool()
		{
			VkDescriptorPoolSize poolSize;
			poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			poolSize.descriptorCount = maxFrameInFlight;

			VkDescriptorPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.poolSizeCount = 1;
			poolInfo.pPoolSizes = &poolSize;
			poolInfo.maxSets = maxFrameInFlight;

			CheckVulkanResult(vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool));
		}

		void CreateDescriptorSet()
		{
			// use layout to crate descriptor set
			std::vector<VkDescriptorSetLayout> layouts(maxFrameInFlight, descriptorSetLayout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = maxFrameInFlight;
			allocInfo.pSetLayouts = layouts.data();

			descriptorSets.resize(maxFrameInFlight);
			CheckVulkanResult(vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()));

			for (size_t i = 0; i < maxFrameInFlight; i++)
			{
				VkDescriptorImageInfo imageInfo{};
				imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfo.imageView = textureImageView;
				imageInfo.sampler = textureSampler;

				VkWriteDescriptorSet descriptorWrite{};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = descriptorSets[i];
				descriptorWrite.dstBinding = 0;
				descriptorWrite.dstArrayElement = 0;
				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				descriptorWrite.descriptorCount = 1;
				descriptorWrite.pImageInfo = &imageInfo;

				// bind descriptor set and uniform buffer
				vkUpdateDescriptorSets(logicalDevice, 1, &descriptorWrite, 0, nullptr);
			}
		}

	private:
		VkSampler textureSampler;
		VkImageView textureImageView;

		uint32_t maxFrameInFlight;
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
		VkDescriptorSetLayout descriptorSetLayout;
		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> descriptorSets;

	};

	VkShaderModule CreateShaderModule(VkDevice logicalDevice, const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
		VkShaderModule shaderModule;
		CheckVulkanResult(vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule));
		return shaderModule;
	}
}

// image
namespace VkUtils
{
	inline bool HasStencilComponent(VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	inline void CreateImage(VkPhysicalDevice physicalDevice,
		VkDevice logicalDevice, uint32_t imageWidth, uint32_t imageHeight, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties,
		VkImage& image, VkDeviceMemory& imageMemory)
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = imageWidth;
		imageInfo.extent.height = imageHeight;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.flags = 0; // Optional
		CheckVulkanResult(vkCreateImage(logicalDevice, &imageInfo, nullptr, &image));

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = FindAppropriateMemoryType(physicalDevice, memRequirements.memoryTypeBits, memoryProperties);
		CheckVulkanResult(vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory));
		CheckVulkanResult(vkBindImageMemory(logicalDevice, image, imageMemory, 0));
	}

	inline void TransitionImageLayout(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
		VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkPipelineStageFlags srcStageMask,VkPipelineStageFlags dstStageMask)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(logicalDevice, commandPool);

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (HasStencilComponent(format))
			{
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) 
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

		EndSingleTimeCommands(logicalDevice, commandPool, commandBuffer, graphicsQueue);
	}

	inline void CopyBufferToImage(VkDevice logicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, 
		VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkCommandBuffer commandBuffer = BeginSingleTimeCommands(logicalDevice, commandPool);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width,height,1 };

		vkCmdCopyBufferToImage(commandBuffer,buffer,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);

		EndSingleTimeCommands(logicalDevice, commandPool, commandBuffer, graphicsQueue);
	}

	// It describes how to access the image and which part of the image to access,
	// for example if it should be treated as a 2D texture depth texture without any mipmapping levels.
	inline VkImageView CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageView textureImageView;

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.aspectMask = aspectFlags;

		CheckVulkanResult(vkCreateImageView(logicalDevice, &viewInfo, nullptr, &textureImageView));

		return textureImageView;
	}

	inline VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {

		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}
}

// device
namespace VkUtils
{
	inline bool CheckDeviceExtensionSupport(VkPhysicalDevice device, std::set<std::string>& requiredExtensions)
	{
		uint32_t extensionCount;
		CheckVulkanResult(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr));

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		CheckVulkanResult(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data()));

		for (const auto& extension : availableExtensions) {
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

	inline VkUtils::QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface)
	{
		VkUtils::QueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

		uint32_t i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			// graphic queue
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
				indices.graphicsFamily = i;

			// present queue
			VkBool32 presentSupport = false;
			CheckVulkanResult(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport));
			if (presentSupport)
				indices.presentFamily = i;

			if (indices.isComplete()) break;

			i++;
		}

		return indices;
	}
}

namespace VkUtils
{
	inline VkImageViewCreateInfo CreateImageViewCreateInfo()
	{
		VkImageViewCreateInfo imageViewCreateInfo{};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		return imageViewCreateInfo;
	}

	inline VkSamplerCreateInfo CreateSamplerCreateInfo()
	{
		VkSamplerCreateInfo samplerCreateInfo{};
		samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCreateInfo.maxAnisotropy = 1.0f;
		return samplerCreateInfo;
	}

	inline VkDescriptorPoolSize CreateDescriptorPoolSize(
		VkDescriptorType type,
		uint32_t descriptorCount)
	{
		VkDescriptorPoolSize descriptorPoolSize{};
		descriptorPoolSize.type = type;
		descriptorPoolSize.descriptorCount = descriptorCount;
		return descriptorPoolSize;
	}

	inline VkDescriptorPoolCreateInfo CreateDescriptorPoolCreateInfo(
		const std::vector<VkDescriptorPoolSize>& poolSizes,
		uint32_t maxSets)
	{
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		descriptorPoolInfo.maxSets = maxSets;
		return descriptorPoolInfo;
	}

	inline VkDescriptorSetLayoutBinding CreateDescriptorSetLayoutBinding(
		VkDescriptorType type,
		VkShaderStageFlags stageFlags,
		uint32_t binding,
		uint32_t descriptorCount = 1)
	{
		VkDescriptorSetLayoutBinding setLayoutBinding{};
		setLayoutBinding.descriptorType = type;
		setLayoutBinding.stageFlags = stageFlags;
		setLayoutBinding.binding = binding;
		setLayoutBinding.descriptorCount = descriptorCount;
		return setLayoutBinding;
	}

	inline VkDescriptorSetLayoutCreateInfo CreateDescriptorSetLayoutCreateInfo(
		const VkDescriptorSetLayoutBinding* pBindings,
		uint32_t bindingCount)
	{
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.pBindings = pBindings;
		descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
		return descriptorSetLayoutCreateInfo;
	}

	inline VkDescriptorSetLayoutCreateInfo CreateDescriptorSetLayoutCreateInfo(
		const std::vector<VkDescriptorSetLayoutBinding>& bindings)
	{
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
		descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorSetLayoutCreateInfo.pBindings = bindings.data();
		descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		return descriptorSetLayoutCreateInfo;
	}

	inline VkDescriptorSetAllocateInfo CreateDescriptorSetAllocateInfo(
		VkDescriptorPool descriptorPool,
		const VkDescriptorSetLayout* pSetLayouts,
		uint32_t descriptorSetCount)
	{
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = descriptorPool;
		descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
		descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
		return descriptorSetAllocateInfo;
	}

	inline VkDescriptorImageInfo CreateDescriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
	{
		VkDescriptorImageInfo descriptorImageInfo{};
		descriptorImageInfo.sampler = sampler;
		descriptorImageInfo.imageView = imageView;
		descriptorImageInfo.imageLayout = imageLayout;
		return descriptorImageInfo;
	}

	inline VkWriteDescriptorSet CreateWriteDescriptorSet(
		VkDescriptorSet dstSet,
		VkDescriptorType type,
		uint32_t binding,
		VkDescriptorBufferInfo* bufferInfo,
		uint32_t descriptorCount = 1)
	{
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = dstSet;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.pBufferInfo = bufferInfo;
		writeDescriptorSet.descriptorCount = descriptorCount;
		return writeDescriptorSet;
	}

	inline VkWriteDescriptorSet CreateWriteDescriptorSet(
		VkDescriptorSet dstSet,
		VkDescriptorType type,
		uint32_t binding,
		VkDescriptorImageInfo* imageInfo,
		uint32_t descriptorCount = 1)
	{
		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = dstSet;
		writeDescriptorSet.descriptorType = type;
		writeDescriptorSet.dstBinding = binding;
		writeDescriptorSet.pImageInfo = imageInfo;
		writeDescriptorSet.descriptorCount = descriptorCount;
		return writeDescriptorSet;
	}

	inline VkPushConstantRange PushConstantRange(
		VkShaderStageFlags stageFlags,
		uint32_t size,
		uint32_t offset)
	{
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = stageFlags;
		pushConstantRange.offset = offset;
		pushConstantRange.size = size;
		return pushConstantRange;
	}

	inline VkBindSparseInfo CreateBindSparseInfo()
	{
		VkBindSparseInfo bindSparseInfo{};
		bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
		return bindSparseInfo;
	}

	inline VkPipelineLayoutCreateInfo CreatePipelineLayoutCreateInfo(
		const VkDescriptorSetLayout* pSetLayouts,
		uint32_t setLayoutCount = 1)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
		pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
		return pipelineLayoutCreateInfo;
	}

	inline VkPipelineLayoutCreateInfo CreatePipelineLayoutCreateInfo(
		uint32_t setLayoutCount = 1)
	{
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
		return pipelineLayoutCreateInfo;
	}

	inline VkPipelineInputAssemblyStateCreateInfo CreatePipelineInputAssemblyStateCreateInfo(
		VkPrimitiveTopology topology,
		VkPipelineInputAssemblyStateCreateFlags flags,
		VkBool32 primitiveRestartEnable)
	{
		VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{};
		pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		pipelineInputAssemblyStateCreateInfo.topology = topology;
		pipelineInputAssemblyStateCreateInfo.flags = flags;
		pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
		return pipelineInputAssemblyStateCreateInfo;
	}

	inline VkPipelineRasterizationStateCreateInfo CreatePipelineRasterizationStateCreateInfo(
		VkPolygonMode polygonMode,
		VkCullModeFlags cullMode,
		VkFrontFace frontFace,
		VkPipelineRasterizationStateCreateFlags flags = 0)
	{
		VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{};
		pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
		pipelineRasterizationStateCreateInfo.cullMode = cullMode;
		pipelineRasterizationStateCreateInfo.frontFace = frontFace;
		pipelineRasterizationStateCreateInfo.flags = flags;
		pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
		pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
		return pipelineRasterizationStateCreateInfo;
	}

	inline VkPipelineColorBlendAttachmentState CreatePipelineColorBlendAttachmentState(
		VkColorComponentFlags colorWriteMask,
		VkBool32 blendEnable)
	{
		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
		pipelineColorBlendAttachmentState.colorWriteMask = colorWriteMask;
		pipelineColorBlendAttachmentState.blendEnable = blendEnable;
		return pipelineColorBlendAttachmentState;
	}

	inline VkPipelineColorBlendStateCreateInfo CreatePipelineColorBlendStateCreateInfo(
		uint32_t attachmentCount,
		const VkPipelineColorBlendAttachmentState* pAttachments)
	{
		VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{};
		pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
		pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
		return pipelineColorBlendStateCreateInfo;
	}

	inline VkPipelineDepthStencilStateCreateInfo CreatePipelineDepthStencilStateCreateInfo(
		VkBool32 depthTestEnable,
		VkBool32 depthWriteEnable,
		VkCompareOp depthCompareOp)
	{
		VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{};
		pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
		pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
		pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
		pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
		return pipelineDepthStencilStateCreateInfo;
	}

	inline VkPipelineViewportStateCreateInfo CreatePipelineViewportStateCreateInfo(
		uint32_t viewportCount,
		uint32_t scissorCount,
		VkPipelineViewportStateCreateFlags flags = 0)
	{
		VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{};
		pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		pipelineViewportStateCreateInfo.viewportCount = viewportCount;
		pipelineViewportStateCreateInfo.scissorCount = scissorCount;
		pipelineViewportStateCreateInfo.flags = flags;
		return pipelineViewportStateCreateInfo;
	}

	inline VkPipelineMultisampleStateCreateInfo CreatePipelineMultisampleStateCreateInfo(
		VkSampleCountFlagBits rasterizationSamples,
		VkPipelineMultisampleStateCreateFlags flags = 0)
	{
		VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{};
		pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		pipelineMultisampleStateCreateInfo.rasterizationSamples = rasterizationSamples;
		pipelineMultisampleStateCreateInfo.flags = flags;
		return pipelineMultisampleStateCreateInfo;
	}

	inline VkPipelineDynamicStateCreateInfo CreatePipelineDynamicStateCreateInfo(
		const VkDynamicState* pDynamicStates,
		uint32_t dynamicStateCount,
		VkPipelineDynamicStateCreateFlags flags = 0)
	{
		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
		pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates;
		pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
		pipelineDynamicStateCreateInfo.flags = flags;
		return pipelineDynamicStateCreateInfo;
	}

	inline VkPipelineDynamicStateCreateInfo CreatePipelineDynamicStateCreateInfo(
		const std::vector<VkDynamicState>& pDynamicStates,
		VkPipelineDynamicStateCreateFlags flags = 0)
	{
		VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
		pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates.data();
		pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(pDynamicStates.size());
		pipelineDynamicStateCreateInfo.flags = flags;
		return pipelineDynamicStateCreateInfo;
	}

	inline VkPipelineTessellationStateCreateInfo CreatePipelineTessellationStateCreateInfo(uint32_t patchControlPoints)
	{
		VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo{};
		pipelineTessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
		pipelineTessellationStateCreateInfo.patchControlPoints = patchControlPoints;
		return pipelineTessellationStateCreateInfo;
	}

	inline VkGraphicsPipelineCreateInfo CreatePipelineCreateInfo(
		VkPipelineLayout layout,
		VkRenderPass renderPass,
		VkPipelineCreateFlags flags = 0)
	{
		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = layout;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.flags = flags;
		pipelineCreateInfo.basePipelineIndex = -1;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		return pipelineCreateInfo;
	}

	inline VkGraphicsPipelineCreateInfo CreatePipelineCreateInfo()
	{
		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.basePipelineIndex = -1;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		return pipelineCreateInfo;
	}

	inline VkVertexInputBindingDescription CreateVertexInputBindingDescription(
		uint32_t binding,
		uint32_t stride,
		VkVertexInputRate inputRate)
	{
		VkVertexInputBindingDescription vInputBindDescription{};
		vInputBindDescription.binding = binding;
		vInputBindDescription.stride = stride;
		vInputBindDescription.inputRate = inputRate;
		return vInputBindDescription;
	}

	inline VkVertexInputAttributeDescription CreateVertexInputAttributeDescription(
		uint32_t binding,
		uint32_t location,
		VkFormat format,
		uint32_t offset)
	{
		VkVertexInputAttributeDescription vInputAttribDescription{};
		vInputAttribDescription.location = location;
		vInputAttribDescription.binding = binding;
		vInputAttribDescription.format = format;
		vInputAttribDescription.offset = offset;
		return vInputAttribDescription;
	}

	inline VkPipelineVertexInputStateCreateInfo CreatePipelineVertexInputStateCreateInfo()
	{
		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		return pipelineVertexInputStateCreateInfo;
	}

	inline VkPipelineVertexInputStateCreateInfo CreatePipelineVertexInputStateCreateInfo(
		const std::vector<VkVertexInputBindingDescription>& vertexBindingDescriptions,
		const std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions
	)
	{
		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindingDescriptions.size());
		pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vertexBindingDescriptions.data();
		pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
		pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();
		return pipelineVertexInputStateCreateInfo;
	}

	inline VkViewport CreateViewport(
		float width,
		float height,
		float minDepth,
		float maxDepth)
	{
		VkViewport viewport{};
		viewport.width = width;
		viewport.height = height;
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		return viewport;
	}

	inline VkRect2D CreateRect2D(
		int32_t width,
		int32_t height,
		int32_t offsetX,
		int32_t offsetY)
	{
		VkRect2D rect2D{};
		rect2D.extent.width = width;
		rect2D.extent.height = height;
		rect2D.offset.x = offsetX;
		rect2D.offset.y = offsetY;
		return rect2D;
	}
}