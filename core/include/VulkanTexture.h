#pragma once

#include <fstream>
#include <cstdlib>
#include <string>
#include <vector>

#include <vulkan/vulkan_core.h>
#include <VulkanDevice.h>
#include <ktx.h>
#include <ktxvulkan.h>

namespace tinygltf
{
	struct Image;
}

namespace vks
{
	class Texture
	{
	public:
		vks::VulkanDevice *   device = nullptr;
		VkImage               image = VK_NULL_HANDLE;
		VkImageLayout         imageLayout;
		VkDeviceMemory        deviceMemory = VK_NULL_HANDLE;
		VkImageView           view = VK_NULL_HANDLE;
		uint32_t              width, height;
		uint32_t              mipLevels;
		uint32_t              layerCount;
		VkDescriptorImageInfo descriptor;
		VkSampler             sampler = VK_NULL_HANDLE;

		void      UpdateDescriptor();
		void      Destroy();
		ktxResult LoadKTXFile(std::string filename, ktxTexture **target);
		virtual bool LoadFromHDRFile(const std::string& fileName);
	};

	class Texture2D : public Texture
	{
	public:
		
		bool LoadFromHDRFile(const std::string& fileName) override;
		
		void LoadFromKtxFile(
			std::string        filename,
			VkFormat           format,
			vks::VulkanDevice *device,
			VkQueue            copyQueue,
			VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			bool               forceLinear     = false);
		
		void FromBuffer(
			void *             buffer,
			VkDeviceSize       bufferSize,
			VkFormat           format,
			uint32_t           texWidth,
			uint32_t           texHeight,
			vks::VulkanDevice *device,
			VkQueue            copyQueue,
			VkFilter           filter          = VK_FILTER_LINEAR,
			VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	};

	class Texture2DArray : public Texture
	{
	public:
		void LoadFromKtxFile(
			std::string        filename,
			VkFormat           format,
			vks::VulkanDevice *device,
			VkQueue            copyQueue,
			VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	};

	class TextureCubeMap : public Texture
	{
	public:
		void LoadFromKtxFile(
			std::string        filename,
			VkFormat           format,
			vks::VulkanDevice *device,
			VkQueue            copyQueue,
			VkImageUsageFlags  imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
			VkImageLayout      imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	};
}     // namespace vks
