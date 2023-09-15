#pragma once
#include <fstream>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <optional>

#include "VkHelper.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
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

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0; // Optional
		copyRegion.dstOffset = 0; // Optional
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		CheckVulkanResult(vkEndCommandBuffer(commandBuffer));

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		CheckVulkanResult(vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
		CheckVulkanResult(vkQueueWaitIdle(graphicsQueue));

		vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
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
			VkDescriptorPoolSize poolSize{};
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
}