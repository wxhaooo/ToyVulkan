#pragma once
#include <cstdint>
#include <stdexcept>
#include <vulkan/vulkan_core.h>

#include "VkUtils.h"

#define STB_IMAGE_IMPLEMENTATION
#include"ThirdParty/image/stb_image.h"

class Image
{
public:
	Image() = delete;
	Image(VkPhysicalDevice physicalDeviceIn, VkDevice logicalDeviceIn,
		VkCommandPool commandPoolIn, VkQueue graphicsQueueIn)
	:physicalDevice(physicalDeviceIn), logicalDevice(logicalDeviceIn),
	commandPool(commandPoolIn),graphicsQueue(graphicsQueueIn)
	{
		
	}
	~Image()
	{
		CleanUp();
	}

public:
	// alpha channel is necessary
	void LoadSRGBImage(std::string imagePath)
	{
		int width = 0, height = 0, channels = 0;
		stbi_uc* pixels = stbi_load(imagePath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

		imageWidth = width; imageHeight = height; imageChannels = channels;
		VkDeviceSize imageSize = imageWidth * imageHeight * 4;

		if (!pixels) {
			throw std::runtime_error("failed to load texture image!");
		}

		// staging buffer
		VkUtils::CreateBuffer(physicalDevice, logicalDevice, imageSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);
		void* data;
		CheckVulkanResult(vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data));
		memcpy(data, pixels, imageSize);
		vkUnmapMemory(logicalDevice, stagingBufferMemory);
		stbi_image_free(pixels);

		// create image
		VkUtils::CreateImage(physicalDevice, logicalDevice, imageWidth, imageHeight, 
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

		VkUtils::TransitionImageLayout(logicalDevice, commandPool, graphicsQueue, textureImage,
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkUtils::CopyBufferToImage(logicalDevice, commandPool, graphicsQueue,
			stagingBuffer, textureImage, imageWidth, imageHeight);

		VkUtils::TransitionImageLayout(logicalDevice, commandPool, graphicsQueue, textureImage,
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

		// create image view
		textureImageView = VkUtils::CreateImageView(logicalDevice, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
		// create sampler
		CreateSampler();
		// create sampler proxy
		textureSampler = std::make_unique<VkUtils::Sampler>(textureImageView, rawTextureSampler, physicalDevice, logicalDevice, VkUtils::MaxFrameInFlight);
	}

	VkImageView GetImageView() { return textureImageView; }
	VkUtils::Sampler* GetTextureSampler() { return textureSampler.get(); }

private:

	void CleanUp()
	{
		textureSampler.reset();
		vkDestroySampler(logicalDevice, rawTextureSampler, nullptr);
		vkDestroyImageView(logicalDevice, textureImageView, nullptr);

		vkDestroyImage(logicalDevice, textureImage, nullptr);
		vkFreeMemory(logicalDevice, textureImageMemory, nullptr);
	}

	void CreateSampler()
	{
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		CheckVulkanResult(vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &rawTextureSampler));
	}

private:

	std::unique_ptr<VkUtils::Sampler> textureSampler;

	VkSampler rawTextureSampler;
	VkImageView textureImageView;

	VkImage textureImage;
	VkDeviceMemory textureImageMemory;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	uint32_t imageWidth;
	uint32_t imageHeight;
	uint32_t imageChannels;

	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
	VkCommandPool commandPool;
	VkQueue graphicsQueue;

public:
	void LoadDefaultImage()
	{
		LoadSRGBImage("./Texture/texture.jpg");
	}
};
