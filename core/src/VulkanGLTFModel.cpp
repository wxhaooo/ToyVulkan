#include <VulkanGLTFModel.h>
#include <VulkanHelper.h>
#include <VulkanInitializers.h>
#include <VulkanUtils.h>
#include <tiny_gltf.h>

#ifdef WIN32
#undef min
#undef max
#endif

namespace vks
{
	namespace geometry
	{
		VkDescriptorSetLayout descriptorSetLayoutImage = VK_NULL_HANDLE;
		VkDescriptorSetLayout descriptorSetLayoutUbo = VK_NULL_HANDLE;
		VkMemoryPropertyFlags memoryPropertyFlags = 0;
		uint32_t descriptorBindingFlags = DescriptorBindingFlags::ImageBaseColor;

		void LoadTextureFromGLTFImage(Texture* texture, tinygltf::Image& gltfImage, std::string path, vks::VulkanDevice* device, VkQueue copyQueue)
		{
			texture->device = device;

			bool isKtx = false;
			// Image points to an external ktx file
			if (gltfImage.uri.find_last_of(".") != std::string::npos) {
				if (gltfImage.uri.substr(gltfImage.uri.find_last_of(".") + 1) == "ktx") {
					isKtx = true;
				}
			}

			VkFormat format;

			if (!isKtx) {
				// Texture was loaded using STB_Image

				unsigned char* buffer = nullptr;
				VkDeviceSize bufferSize = 0;
				bool deleteBuffer = false;
				if (gltfImage.component == 3) {
					// Most devices don't support RGB only on Vulkan so convert if necessary
					// TODO: Check actual format support and transform only if required
					bufferSize = gltfImage.width * gltfImage.height * 4;
					buffer = new unsigned char[bufferSize];
					unsigned char* rgba = buffer;
					unsigned char* rgb = &gltfImage.image[0];
					for (size_t i = 0; i < gltfImage.width * gltfImage.height; ++i) {
						for (int32_t j = 0; j < 3; ++j) {
							rgba[j] = rgb[j];
						}
						rgba += 4;
						rgb += 3;
					}
					deleteBuffer = true;
				}
				else {
					buffer = &gltfImage.image[0];
					bufferSize = gltfImage.image.size();
				}

				format = VK_FORMAT_R8G8B8A8_UNORM;

				VkFormatProperties formatProperties;

				texture->width = gltfImage.width;
				texture->height = gltfImage.height;
				texture->mipLevels = static_cast<uint32_t>(floor(log2(std::max(texture->width, texture->height))) + 1.0);

				vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);
				assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT);
				assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);

				VkMemoryAllocateInfo memAllocInfo{};
				memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
				VkMemoryRequirements memReqs{};

				VkBuffer stagingBuffer;
				VkDeviceMemory stagingMemory;

				VkBufferCreateInfo bufferCreateInfo{};
				bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
				bufferCreateInfo.size = bufferSize;
				bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				CheckVulkanResult(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));
				vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);
				memAllocInfo.allocationSize = memReqs.size;
				memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				CheckVulkanResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
				CheckVulkanResult(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

				uint8_t* data;
				CheckVulkanResult(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
				memcpy(data, buffer, bufferSize);
				vkUnmapMemory(device->logicalDevice, stagingMemory);

				VkImageCreateInfo imageCreateInfo{};
				imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.format = format;
				imageCreateInfo.mipLevels = texture->mipLevels;
				imageCreateInfo.arrayLayers = 1;
				imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
				imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageCreateInfo.extent = { texture->width, texture->height, 1 };
				imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
				CheckVulkanResult(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &texture->image));
				vkGetImageMemoryRequirements(device->logicalDevice, texture->image, &memReqs);
				memAllocInfo.allocationSize = memReqs.size;
				memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				CheckVulkanResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &texture->deviceMemory));
				CheckVulkanResult(vkBindImageMemory(device->logicalDevice, texture->image, texture->deviceMemory, 0));

				VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

				VkImageSubresourceRange subresourceRange = {};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.levelCount = 1;
				subresourceRange.layerCount = 1;

				VkImageMemoryBarrier imageMemoryBarrier{};

				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageMemoryBarrier.srcAccessMask = 0;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.image = texture->image;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

				VkBufferImageCopy bufferCopyRegion = {};
				bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				bufferCopyRegion.imageSubresource.mipLevel = 0;
				bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
				bufferCopyRegion.imageSubresource.layerCount = 1;
				bufferCopyRegion.imageExtent.width = texture->width;
				bufferCopyRegion.imageExtent.height = texture->height;
				bufferCopyRegion.imageExtent.depth = 1;

				vkCmdCopyBufferToImage(copyCmd, stagingBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				imageMemoryBarrier.image = texture->image;
				imageMemoryBarrier.subresourceRange = subresourceRange;
  				vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

				device->FlushCommandBuffer(copyCmd, copyQueue, true);

				vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
				vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

				// Generate the mip chain (glTF uses jpg and png, so we need to create this manually)
				VkCommandBuffer blitCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
				for (uint32_t i = 1; i < texture->mipLevels; i++) {
					VkImageBlit imageBlit{};

					imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageBlit.srcSubresource.layerCount = 1;
					imageBlit.srcSubresource.mipLevel = i - 1;
					imageBlit.srcOffsets[1].x = int32_t(texture->width >> (i - 1));
					imageBlit.srcOffsets[1].y = int32_t(texture->height >> (i - 1));
					imageBlit.srcOffsets[1].z = 1;

					imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imageBlit.dstSubresource.layerCount = 1;
					imageBlit.dstSubresource.mipLevel = i;
					imageBlit.dstOffsets[1].x = int32_t(texture->width >> i);
					imageBlit.dstOffsets[1].y = int32_t(texture->height >> i);
					imageBlit.dstOffsets[1].z = 1;

					VkImageSubresourceRange mipSubRange = {};
					mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					mipSubRange.baseMipLevel = i;
					mipSubRange.levelCount = 1;
					mipSubRange.layerCount = 1;

					{
						VkImageMemoryBarrier imageMemoryBarrier{};
						imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						imageMemoryBarrier.srcAccessMask = 0;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						imageMemoryBarrier.image = texture->image;
						imageMemoryBarrier.subresourceRange = mipSubRange;
						vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
					}

					vkCmdBlitImage(blitCmd, texture->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

					{
						VkImageMemoryBarrier imageMemoryBarrier{};
						imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.image = texture->image;
						imageMemoryBarrier.subresourceRange = mipSubRange;
						vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
					}
				}

				subresourceRange.levelCount = texture->mipLevels;
				texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageMemoryBarrier.image = texture->image;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

		        if (deleteBuffer) {
		            delete[] buffer;
		        }

				device->FlushCommandBuffer(blitCmd, copyQueue, true);
			}
			else {
				// Texture is stored in an external ktx file
				std::string filename = path + "/" + gltfImage.uri;

				ktxTexture* ktxTexture;

				ktxResult result = KTX_SUCCESS;
		#if defined(__ANDROID__)
				AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
				if (!asset) {
					vks::tools::exitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
				}
				size_t size = AAsset_getLength(asset);
				assert(size > 0);
				ktx_uint8_t* textureData = new ktx_uint8_t[size];
				AAsset_read(asset, textureData, size);
				AAsset_close(asset);
				result = ktxTexture_CreateFromMemory(textureData, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
				delete[] textureData;
		#else
				if (!vks::helper::FileExists(filename)) {
					vks::helper::ExitFatal("Could not load texture from " + filename + "\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
				}
				result = ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
		#endif		
				assert(result == KTX_SUCCESS);

				texture->device = device;
				texture->width = ktxTexture->baseWidth;
				texture->height = ktxTexture->baseHeight;
				texture->mipLevels = ktxTexture->numLevels;

				ktx_uint8_t* ktxTextureData = ktxTexture_GetData(ktxTexture);
				ktx_size_t ktxTextureSize = ktxTexture_GetSize(ktxTexture);
				// @todo: Use ktxTexture_GetVkFormat(ktxTexture)
				format = VK_FORMAT_R8G8B8A8_UNORM;

				// Get device properties for the requested texture format
				VkFormatProperties formatProperties;
				vkGetPhysicalDeviceFormatProperties(device->physicalDevice, format, &formatProperties);

				VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
				VkBuffer stagingBuffer;
				VkDeviceMemory stagingMemory;

				VkBufferCreateInfo bufferCreateInfo = vks::initializers::BufferCreateInfo();
				bufferCreateInfo.size = ktxTextureSize;
				// This buffer is used as a transfer source for the buffer copy
				bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				CheckVulkanResult(vkCreateBuffer(device->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

				VkMemoryAllocateInfo memAllocInfo = vks::initializers::MemoryAllocateInfo();
				VkMemoryRequirements memReqs;
				vkGetBufferMemoryRequirements(device->logicalDevice, stagingBuffer, &memReqs);
				memAllocInfo.allocationSize = memReqs.size;
				memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
				CheckVulkanResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
				CheckVulkanResult(vkBindBufferMemory(device->logicalDevice, stagingBuffer, stagingMemory, 0));

				uint8_t* data;
				CheckVulkanResult(vkMapMemory(device->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
				memcpy(data, ktxTextureData, ktxTextureSize);
				vkUnmapMemory(device->logicalDevice, stagingMemory);

				std::vector<VkBufferImageCopy> bufferCopyRegions;
				for (uint32_t i = 0; i < texture->mipLevels; i++)
				{
					ktx_size_t offset;
					KTX_error_code result = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
					assert(result == KTX_SUCCESS);
					VkBufferImageCopy bufferCopyRegion = {};
					bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					bufferCopyRegion.imageSubresource.mipLevel = i;
					bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
					bufferCopyRegion.imageSubresource.layerCount = 1;
					bufferCopyRegion.imageExtent.width = std::max(1u, ktxTexture->baseWidth >> i);
					bufferCopyRegion.imageExtent.height = std::max(1u, ktxTexture->baseHeight >> i);
					bufferCopyRegion.imageExtent.depth = 1;
					bufferCopyRegion.bufferOffset = offset;
					bufferCopyRegions.push_back(bufferCopyRegion);
				}

				// Create optimal tiled target image
				VkImageCreateInfo imageCreateInfo = vks::initializers::ImageCreateInfo();
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.format = format;
				imageCreateInfo.mipLevels = texture->mipLevels;
				imageCreateInfo.arrayLayers = 1;
				imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageCreateInfo.extent = { texture->width, texture->height, 1 };
				imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
				CheckVulkanResult(vkCreateImage(device->logicalDevice, &imageCreateInfo, nullptr, &texture->image));

				vkGetImageMemoryRequirements(device->logicalDevice, texture->image, &memReqs);
				memAllocInfo.allocationSize = memReqs.size;
				memAllocInfo.memoryTypeIndex = device->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
				CheckVulkanResult(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &texture->deviceMemory));
				CheckVulkanResult(vkBindImageMemory(device->logicalDevice, texture->image, texture->deviceMemory, 0));

				VkImageSubresourceRange subresourceRange = {};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.levelCount = texture->mipLevels;
				subresourceRange.layerCount = 1;

				vks::utils::SetImageLayout(copyCmd, texture->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
				vkCmdCopyBufferToImage(copyCmd, stagingBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());
				vks::utils::SetImageLayout(copyCmd, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
				device->FlushCommandBuffer(copyCmd, copyQueue);
				texture->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				vkDestroyBuffer(device->logicalDevice, stagingBuffer, nullptr);
				vkFreeMemory(device->logicalDevice, stagingMemory, nullptr);

				ktxTexture_Destroy(ktxTexture);
			}

			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
			samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
			samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerInfo.maxAnisotropy = device->enabledFeatures.samplerAnisotropy ? device->properties.limits.maxSamplerAnisotropy : 1.0f;
			samplerInfo.anisotropyEnable = device->enabledFeatures.samplerAnisotropy;
			samplerInfo.maxLod = (float)texture->mipLevels;
			samplerInfo.maxAnisotropy = 8.0f;
			CheckVulkanResult(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &texture->sampler));

			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.image = texture->image;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.format = format;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.layerCount = 1;
			viewInfo.subresourceRange.levelCount = texture->mipLevels;
			CheckVulkanResult(vkCreateImageView(device->logicalDevice, &viewInfo, nullptr, &texture->view));

			texture->descriptor.sampler = texture->sampler;
			texture->descriptor.imageView = texture->view;
			texture->descriptor.imageLayout = texture->imageLayout;
		}
		
		/*
		We use a custom image loading function with tinyglTF, so we can do custom stuff loading ktx textures
		*/
		bool LoadImageDataFunc(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
		{
			// KTX files will be handled by our own code
			auto formatPos = image->uri.find_last_of(".");
			if (formatPos != std::string::npos)
			{
				std::string imageFileFormat = image->uri.substr(formatPos + 1);
				if (imageFileFormat == "ktx")
					return true;
			}

			// default .png & .jpg file
			return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
		}

		bool LoadImageDataFuncEmpty(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData) 
		{
			// This function will be used for samples that don't require images to be loaded
			return true;
		}
		
		void VulkanGLTFModel::Material::Destory()
		{
			if(baseColorTexture) baseColorTexture->Destroy();
			if(metallicRoughnessTexture) metallicRoughnessTexture->Destroy();
			if(normalTexture) normalTexture->Destroy();
			if(occlusionTexture) occlusionTexture->Destroy();
			if(emissiveTexture) emissiveTexture->Destroy();
			if(specularGlossinessTexture) specularGlossinessTexture->Destroy();
			if(diffuseTexture) diffuseTexture->Destroy();
		}
		
		void VulkanGLTFModel::Material::CreateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags)
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
			descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocInfo.descriptorPool = descriptorPool;
			descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayout;
			descriptorSetAllocInfo.descriptorSetCount = 1;
			CheckVulkanResult(vkAllocateDescriptorSets(device->logicalDevice, &descriptorSetAllocInfo, &descriptorSet));
			std::vector<VkDescriptorImageInfo> imageDescriptors{};
			std::vector<VkWriteDescriptorSet> writeDescriptorSets{};
			if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor) {
				imageDescriptors.push_back(baseColorTexture->descriptor);
				VkWriteDescriptorSet writeDescriptorSet{};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = descriptorSet;
				writeDescriptorSet.dstBinding = static_cast<uint32_t>(writeDescriptorSets.size());
				writeDescriptorSet.pImageInfo = &baseColorTexture->descriptor;
				writeDescriptorSets.push_back(writeDescriptorSet);
			}
			if (normalTexture && descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap) {
				imageDescriptors.push_back(normalTexture->descriptor);
				VkWriteDescriptorSet writeDescriptorSet{};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = descriptorSet;
				writeDescriptorSet.dstBinding = static_cast<uint32_t>(writeDescriptorSets.size());
				writeDescriptorSet.pImageInfo = &normalTexture->descriptor;
				writeDescriptorSets.push_back(writeDescriptorSet);
			}
			vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		}
		
		void VulkanGLTFModel::Primitive::SetDimensions(glm::vec3 min, glm::vec3 max) {
			dimensions.min = min;
			dimensions.max = max;
			dimensions.size = max - min;
			dimensions.center = (min + max) / 2.0f;
			dimensions.radius = glm::distance(min, max) / 2.0f;
		}
		
		VulkanGLTFModel::Mesh::Mesh(vks::VulkanDevice *device, glm::mat4 matrix) {
			this->device = device;
			this->uniformBlock.matrix = matrix;
			CheckVulkanResult(device->CreateBuffer(
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				sizeof(uniformBlock),
				&uniformBuffer.buffer,
				&uniformBuffer.memory,
				&uniformBlock));
			CheckVulkanResult(vkMapMemory(device->logicalDevice, uniformBuffer.memory, 0, sizeof(uniformBlock), 0, &uniformBuffer.mapped));
			uniformBuffer.descriptor = { uniformBuffer.buffer, 0, sizeof(uniformBlock) };
		};

		VulkanGLTFModel::Mesh::~Mesh() {
			vkDestroyBuffer(device->logicalDevice, uniformBuffer.buffer, nullptr);
			vkFreeMemory(device->logicalDevice, uniformBuffer.memory, nullptr);
			for(auto primitive : primitives)
			{
				delete primitive;
			}
		}

		glm::mat4 VulkanGLTFModel::Node::LocalMatrix() {
			return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
		}

		glm::mat4 VulkanGLTFModel::Node::GetMatrix() {
			glm::mat4 m = LocalMatrix();
			VulkanGLTFModel::Node *p = parent;
			while (p) {
				m = p->LocalMatrix() * m;
				p = p->parent;
			}
			return m;
		}

		void VulkanGLTFModel::Node::Update() {
			if (mesh) {
				glm::mat4 m = GetMatrix();
				if (skin) {
					mesh->uniformBlock.matrix = m;
					// Update join matrices
					glm::mat4 inverseTransform = glm::inverse(m);
					for (size_t i = 0; i < skin->joints.size(); i++) {
						VulkanGLTFModel::Node *jointNode = skin->joints[i];
						glm::mat4 jointMat = jointNode->GetMatrix() * skin->inverseBindMatrices[i];
						jointMat = inverseTransform * jointMat;
						mesh->uniformBlock.jointMatrix[i] = jointMat;
					}
					mesh->uniformBlock.jointcount = (float)skin->joints.size();
					memcpy(mesh->uniformBuffer.mapped, &mesh->uniformBlock, sizeof(mesh->uniformBlock));
				} else
					memcpy(mesh->uniformBuffer.mapped, &m, sizeof(glm::mat4));
			}

			for (auto& child : children)
				child->Update();
		}

		VulkanGLTFModel::Node::~Node()
		{
			if (mesh)
				delete mesh;
			
			for (auto& child : children)
				delete child;
		}
		
		// Contains everything required to render a glTF model in Vulkan
		// This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
		VulkanGLTFModel::~VulkanGLTFModel()
		{
			// Release all Vulkan resources allocated for the model
			vkDestroyBuffer(vulkanDevice->logicalDevice, vertices.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, vertices.memory, nullptr);
			vkDestroyBuffer(vulkanDevice->logicalDevice, indices.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, indices.memory, nullptr);

			for (Texture image : textures) 
				image.Destroy();
			
			for (auto node : nodes)
				delete node;

			for (auto skin : skins)
				delete skin;
			
			if (descriptorSetLayoutUbo != VK_NULL_HANDLE) {
				vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayoutUbo, nullptr);
				descriptorSetLayoutUbo = VK_NULL_HANDLE;
			}
			if (descriptorSetLayoutImage != VK_NULL_HANDLE) {
				vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayoutImage, nullptr);
				descriptorSetLayoutImage = VK_NULL_HANDLE;
			}
			vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);
			emptyTexture.Destroy();
		}

		/*
			glTF loading functions
			The following functions take a glTF input model loaded via tinyglTF and convert all required data into our own structure
		*/
		void VulkanGLTFModel::LoadImages(tinygltf::Model& input)
		{
			for(tinygltf::Image &image:input.images)
			{
				vks::Texture texture;
				LoadTextureFromGLTFImage(&texture,image,path,vulkanDevice,copyQueue);
				textures.push_back(texture);
			}

			// Create an empty texture to be used for empty material images
			CreateEmptyTexture(copyQueue);
		}
	
		void VulkanGLTFModel::LoadMaterials(tinygltf::Model& input)
		{
			for (size_t i = 0; i < input.materials.size(); i++)
			{
				tinygltf::Material &mat = input.materials[i];
				Material material(vulkanDevice);
				
				if (mat.values.find("baseColorTexture") != mat.values.end())
					material.baseColorTexture = GetTexture(input.textures[mat.values["baseColorTexture"].TextureIndex()].source);
				// Metallic roughness workflow
				if (mat.values.find("metallicRoughnessTexture") != mat.values.end())
					material.metallicRoughnessTexture = GetTexture(input.textures[mat.values["metallicRoughnessTexture"].TextureIndex()].source);
		
				if (mat.values.find("roughnessFactor") != mat.values.end())
					material.roughnessFactor = static_cast<float>(mat.values["roughnessFactor"].Factor());
		
				if (mat.values.find("metallicFactor") != mat.values.end())
					material.metallicFactor = static_cast<float>(mat.values["metallicFactor"].Factor());
		
				if (mat.values.find("baseColorFactor") != mat.values.end())
					material.baseColorFactor = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());

				if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end())
					material.normalTexture = GetTexture(input.textures[mat.additionalValues["normalTexture"].TextureIndex()].source);
				else
					material.normalTexture = &emptyTexture;
		
				if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
					material.emissiveTexture = GetTexture(input.textures[mat.additionalValues["emissiveTexture"].TextureIndex()].source);
			
				if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
					material.occlusionTexture = GetTexture(input.textures[mat.additionalValues["occlusionTexture"].TextureIndex()].source);
			
				if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end()) {
					tinygltf::Parameter param = mat.additionalValues["alphaMode"];
					if (param.string_value == "BLEND")
						material.alphaMode = Material::ALPHAMODE_BLEND;
					if (param.string_value == "MASK")
						material.alphaMode = Material::ALPHAMODE_MASK;
				}
				if (mat.additionalValues.find("alphaCutoff") != mat.additionalValues.end())
					material.alphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());

				materials.push_back(material);
			}

			// Push a default material at the end of the list for meshes with no material assigned
			materials.push_back(Material(vulkanDevice));
		}
	
		void VulkanGLTFModel::LoadNode(Node *parent, const tinygltf::Node &node, uint32_t nodeIndex, const tinygltf::Model &model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalScale)
		{
			Node *newNode = new Node{};
			newNode->index = nodeIndex;
			newNode->parent = parent;
			newNode->name = node.name;
			newNode->skinIndex = node.skin;
			newNode->matrix = glm::mat4(1.0f);

			// Generate local node matrix
			glm::vec3 translation = glm::vec3(0.0f);
			if (node.translation.size() == 3) {
				translation = glm::make_vec3(node.translation.data());
				newNode->translation = translation;
			}
			glm::mat4 rotation = glm::mat4(1.0f);
			if (node.rotation.size() == 4) {
				glm::quat q = glm::make_quat(node.rotation.data());
				newNode->rotation = glm::mat4(q);
			}
			glm::vec3 scale = glm::vec3(1.0f);
			if (node.scale.size() == 3) {
				scale = glm::make_vec3(node.scale.data());
				newNode->scale = scale;
			}
			if (node.matrix.size() == 16) {
				newNode->matrix = glm::make_mat4x4(node.matrix.data());
				if (globalScale != 1.0f) {
					newNode->matrix = glm::scale(newNode->matrix, glm::vec3(globalScale));
				}
			}

			// Node with children
			if (node.children.size() > 0) {
				for (auto i = 0; i < node.children.size(); i++) {
					LoadNode(newNode, model.nodes[node.children[i]], node.children[i], model, indexBuffer, vertexBuffer, globalScale);
				}
			}

			// Node contains mesh data
			if (node.mesh > -1) {
				const tinygltf::Mesh mesh = model.meshes[node.mesh];
				Mesh *newMesh = new Mesh(vulkanDevice, newNode->matrix);
				newMesh->name = mesh.name;
				for (size_t j = 0; j < mesh.primitives.size(); j++) {
					const tinygltf::Primitive &primitive = mesh.primitives[j];
					if (primitive.indices < 0) {
						continue;
					}
					uint32_t indexStart = static_cast<uint32_t>(indexBuffer.size());
					uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
					uint32_t indexCount = 0;
					uint32_t vertexCount = 0;
					glm::vec3 posMin{};
					glm::vec3 posMax{};
					bool hasSkin = false;
					// Vertices
					{
						const float *bufferPos = nullptr;
						const float *bufferNormals = nullptr;
						const float *bufferTexCoords = nullptr;
						const float* bufferColors = nullptr;
						const float *bufferTangents = nullptr;
						uint32_t numColorComponents;
						const uint16_t *bufferJoints = nullptr;
						const float *bufferWeights = nullptr;

						// Position attribute is required
						assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

						const tinygltf::Accessor &posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
						const tinygltf::BufferView &posView = model.bufferViews[posAccessor.bufferView];
						bufferPos = reinterpret_cast<const float *>(&(model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]));
						posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
						posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);

						if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
							const tinygltf::Accessor &normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
							const tinygltf::BufferView &normView = model.bufferViews[normAccessor.bufferView];
							bufferNormals = reinterpret_cast<const float *>(&(model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]));
						}

						if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
							const tinygltf::Accessor &uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
							const tinygltf::BufferView &uvView = model.bufferViews[uvAccessor.bufferView];
							bufferTexCoords = reinterpret_cast<const float *>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
						}

						if (primitive.attributes.find("COLOR_0") != primitive.attributes.end())
						{
							const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
							const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
							// Color buffer are either of type vec3 or vec4
							numColorComponents = colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
							bufferColors = reinterpret_cast<const float*>(&(model.buffers[colorView.buffer].data[colorAccessor.byteOffset + colorView.byteOffset]));
						}

						if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
						{
							const tinygltf::Accessor &tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
							const tinygltf::BufferView &tangentView = model.bufferViews[tangentAccessor.bufferView];
							bufferTangents = reinterpret_cast<const float *>(&(model.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]));
						}

						// Skinning
						// Joints
						if (primitive.attributes.find("JOINTS_0") != primitive.attributes.end()) {
							const tinygltf::Accessor &jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
							const tinygltf::BufferView &jointView = model.bufferViews[jointAccessor.bufferView];
							bufferJoints = reinterpret_cast<const uint16_t *>(&(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]));
						}

						if (primitive.attributes.find("WEIGHTS_0") != primitive.attributes.end()) {
							const tinygltf::Accessor &uvAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
							const tinygltf::BufferView &uvView = model.bufferViews[uvAccessor.bufferView];
							bufferWeights = reinterpret_cast<const float *>(&(model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]));
						}

						hasSkin = (bufferJoints && bufferWeights);

						vertexCount = static_cast<uint32_t>(posAccessor.count);

						for (size_t v = 0; v < posAccessor.count; v++) {
							Vertex vert{};
							vert.pos = glm::vec4(glm::make_vec3(&bufferPos[v * 3]), 1.0f);
							vert.normal = glm::normalize(glm::vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * 3]) : glm::vec3(0.0f)));
							vert.uv = bufferTexCoords ? glm::make_vec2(&bufferTexCoords[v * 2]) : glm::vec3(0.0f);
							if (bufferColors) {
								switch (numColorComponents) {
									case 3: 
										vert.color = glm::vec4(glm::make_vec3(&bufferColors[v * 3]), 1.0f);
									case 4:
										vert.color = glm::make_vec4(&bufferColors[v * 4]);
								}
							}
							else {
								vert.color = glm::vec4(1.0f);
							}
							vert.tangent = bufferTangents ? glm::vec4(glm::make_vec4(&bufferTangents[v * 4])) : glm::vec4(0.0f);
							vert.joint0 = hasSkin ? glm::vec4(glm::make_vec4(&bufferJoints[v * 4])) : glm::vec4(0.0f);
							vert.weight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * 4]) : glm::vec4(0.0f);
							vertexBuffer.push_back(vert);
						}
					}
					// Indices
					{
						const tinygltf::Accessor &accessor = model.accessors[primitive.indices];
						const tinygltf::BufferView &bufferView = model.bufferViews[accessor.bufferView];
						const tinygltf::Buffer &buffer = model.buffers[bufferView.buffer];

						indexCount = static_cast<uint32_t>(accessor.count);

						switch (accessor.componentType) {
						case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
							uint32_t *buf = new uint32_t[accessor.count];
							memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
							for (size_t index = 0; index < accessor.count; index++) {
								indexBuffer.push_back(buf[index] + vertexStart);
							}
		                    delete[] buf;
							break;
						}
						case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
							uint16_t *buf = new uint16_t[accessor.count];
							memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
							for (size_t index = 0; index < accessor.count; index++) {
								indexBuffer.push_back(buf[index] + vertexStart);
							}
		                    delete[] buf;
		                    break;
						}
						case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
							uint8_t *buf = new uint8_t[accessor.count];
							memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
							for (size_t index = 0; index < accessor.count; index++) {
								indexBuffer.push_back(buf[index] + vertexStart);
							}
		                    delete[] buf;
		                    break;
						}
						default:
							std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
							return;
						}
					}
					Primitive *newPrimitive = new Primitive(indexStart, indexCount, primitive.material > -1 ? materials[primitive.material] : materials.back());
					newPrimitive->firstVertex = vertexStart;
					newPrimitive->vertexCount = vertexCount;
					newPrimitive->SetDimensions(posMin, posMax);
					newMesh->primitives.push_back(newPrimitive);
				}
				newNode->mesh = newMesh;
			}
			if (parent) {
				parent->children.push_back(newNode);
			} else {
				nodes.push_back(newNode);
			}
			linearNodes.push_back(newNode);
		}

		void VulkanGLTFModel::LoadGLTFFile(std::string fileName, VulkanDevice* vulkanDevice, VkQueue transferQueue, uint32_t fileLoadingFlags, float scale)
        {
            tinygltf::Model glTFInput;
			tinygltf::TinyGLTF gltfContext;

			if (fileLoadingFlags & FileLoadingFlags::DontLoadImages)
				gltfContext.SetImageLoader(LoadImageDataFuncEmpty, nullptr);
			else
				gltfContext.SetImageLoader(LoadImageDataFunc, nullptr);

			size_t pos = fileName.find_last_of('/');
			path = fileName.substr(0, pos);

			// Pass some Vulkan resources required for setup and rendering to the glTF model loading class
			this->vulkanDevice = vulkanDevice;
			this->copyQueue = transferQueue;
			
			std::string error, warning;
			bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, fileName);
		
			std::vector<uint32_t> indexBuffer;
			std::vector<geometry::VulkanGLTFModel::Vertex> vertexBuffer;

			if (!fileLoaded)
				helper::ExitFatal("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);

			if (!(fileLoadingFlags & FileLoadingFlags::DontLoadImages))
				LoadImages(glTFInput);
			LoadMaterials(glTFInput);
			const tinygltf::Scene &scene = glTFInput.scenes[glTFInput.defaultScene > -1 ? glTFInput.defaultScene : 0];
			for (size_t i = 0; i < scene.nodes.size(); i++)
			{
				const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
				LoadNode(nullptr, node, scene.nodes[i], glTFInput, indexBuffer, vertexBuffer, scale);
			}
			if (glTFInput.animations.size() > 0) {
				// LoadAnimations(glTFInput);
			}
			// LoadSkins(glTFInput);
			
			for (auto node : linearNodes) {
				// Assign skins
				if (node->skinIndex > -1) {
					node->skin = skins[node->skinIndex];
				}
				// Initial pose
				if (node->mesh) {
					node->Update();
				}
			}

			// Pre-Calculations for requested features
			if ((fileLoadingFlags & FileLoadingFlags::PreTransformVertices) || (fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors) || (fileLoadingFlags & FileLoadingFlags::FlipY)) {
				const bool preTransform = fileLoadingFlags & FileLoadingFlags::PreTransformVertices;
				const bool preMultiplyColor = fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors;
				const bool flipY = fileLoadingFlags & FileLoadingFlags::FlipY;
				for (Node* node : linearNodes) {
					if (node->mesh) {
						const glm::mat4 localMatrix = node->GetMatrix();
						const glm::mat3 localMatrixInvT = transpose(inverse(glm::mat3(localMatrix)));
						for (Primitive* primitive : node->mesh->primitives) {
							for (uint32_t i = 0; i < primitive->vertexCount; i++) {
								Vertex& vertex = vertexBuffer[primitive->firstVertex + i];
								
								// Pre-transform vertex positions by node-hierarchy
								if (preTransform) {
									vertex.pos = glm::vec3(localMatrix * glm::vec4(vertex.pos, 1.0f));
									vertex.normal = normalize(localMatrixInvT * vertex.normal);
								}

								// Flip Y-Axis of vertex positions
								if (flipY) {
									vertex.pos.y *= -1.0f;
									vertex.normal.y *= -1.0f;
								}
								
								// Pre-Multiply vertex colors with material base color
								if (preMultiplyColor) {
									vertex.color = primitive->material.baseColorFactor * vertex.color;
								}
							}
						}
					}
				}
			}

			for (auto extension : glTFInput.extensionsUsed) {
				if (extension == "KHR_materials_pbrSpecularGlossiness") {
					std::cout << "Required extension: " << extension;
					metallicRoughnessWorkflow = false;
				}
			}
		
			// Create and upload vertex and index buffer
			// We will be using one single vertex buffer and one single index buffer for the whole glTF scene
			// Primitives (of the glTF model) will then index into these using index offsets

			size_t vertexBufferSize = vertexBuffer.size() * sizeof(geometry::VulkanGLTFModel::Vertex);
			size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
			indices.count = static_cast<uint32_t>(indexBuffer.size());

			struct StagingBuffer {
				VkBuffer buffer;
				VkDeviceMemory memory;
			} vertexStaging, indexStaging;

			// Create host visible staging buffers (source)
			CheckVulkanResult(vulkanDevice->CreateBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				vertexBufferSize,
				&vertexStaging.buffer,
				&vertexStaging.memory,
				vertexBuffer.data()));
			// Index data
			CheckVulkanResult(vulkanDevice->CreateBuffer(
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				indexBufferSize,
				&indexStaging.buffer,
				&indexStaging.memory,
				indexBuffer.data()));

			// Create device local buffers (target)
			CheckVulkanResult(vulkanDevice->CreateBuffer(
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vertexBufferSize,
				&vertices.buffer,
				&vertices.memory));
			CheckVulkanResult(vulkanDevice->CreateBuffer(
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				indexBufferSize,
				&indices.buffer,
				&indices.memory));

			// Copy data from staging buffers (host) do device local buffer (gpu)
			VkCommandBuffer copyCmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			VkBufferCopy copyRegion = {};

			copyRegion.size = vertexBufferSize;
			vkCmdCopyBuffer(
				copyCmd,
				vertexStaging.buffer,
				vertices.buffer,
				1,
				&copyRegion);

			copyRegion.size = indexBufferSize;
			vkCmdCopyBuffer(
				copyCmd,
				indexStaging.buffer,
				indices.buffer,
				1,
				&copyRegion);

			vulkanDevice->FlushCommandBuffer(copyCmd, transferQueue, true);

			// Free staging resources
			vkDestroyBuffer(vulkanDevice->logicalDevice, vertexStaging.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, vertexStaging.memory, nullptr);
			vkDestroyBuffer(vulkanDevice->logicalDevice, indexStaging.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, indexStaging.memory, nullptr);

			GetSceneDimensions();
			// Setup descriptors
			uint32_t uboCount{ 0 };
			uint32_t imageCount{ 0 };
			for (auto node : linearNodes) {
				if (node->mesh) {
					uboCount++;
				}
			}
			for (auto material : materials) {
				if (material.baseColorTexture != nullptr) {
					imageCount++;
				}
			}
			std::vector<VkDescriptorPoolSize> poolSizes = {
				{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uboCount },
			};
			if (imageCount > 0) {
				if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor) {
					poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount });
				}
				if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap) {
					poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageCount });
				}
			}
			VkDescriptorPoolCreateInfo descriptorPoolCI{};
			descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
			descriptorPoolCI.pPoolSizes = poolSizes.data();
			descriptorPoolCI.maxSets = uboCount + imageCount;
			CheckVulkanResult(vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolCI, nullptr, &descriptorPool));

			// Descriptors for per-node uniform buffers
			{
				// Layout is global, so only create if it hasn't already been created before
				if (descriptorSetLayoutUbo == VK_NULL_HANDLE) {
					std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
						vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
					};
					VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
					descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
					descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
					descriptorLayoutCI.pBindings = setLayoutBindings.data();
					CheckVulkanResult(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayoutUbo));
				}
				for (auto node : nodes) {
					PrepareNodeDescriptor(node, descriptorSetLayoutUbo);
				}
			}

			// Descriptors for per-material images
			{
				// Layout is global, so only create if it hasn't already been created before
				if (descriptorSetLayoutImage == VK_NULL_HANDLE) {
					std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
					if (descriptorBindingFlags & DescriptorBindingFlags::ImageBaseColor) {
						setLayoutBindings.push_back(vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(setLayoutBindings.size())));
					}
					if (descriptorBindingFlags & DescriptorBindingFlags::ImageNormalMap) {
						setLayoutBindings.push_back(vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, static_cast<uint32_t>(setLayoutBindings.size())));
					}
					VkDescriptorSetLayoutCreateInfo descriptorLayoutCI{};
					descriptorLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
					descriptorLayoutCI.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
					descriptorLayoutCI.pBindings = setLayoutBindings.data();
					CheckVulkanResult(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorLayoutCI, nullptr, &descriptorSetLayoutImage));
				}
				for (auto& material : materials) {
					if (material.baseColorTexture != nullptr) {
						material.CreateDescriptorSet(descriptorPool, descriptorSetLayoutImage, descriptorBindingFlags);
					}
				}
			}
        }

		/*
			glTF rendering functions
		*/
		// Draw a single node including child nodes (if present)
		void VulkanGLTFModel::DrawNode(Node* node, VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet)
		{
			if (node->mesh) {
				if (node->mesh->primitives.size() > 0)
				{
					auto nodeMatrix = node->GetMatrix();
					glm::mat4 idMat = glm::mat4(1.0f);
					// Pass the final matrix to the vertex shader using push constants
					// vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &idMat);
					vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
				}
				for (Primitive* primitive : node->mesh->primitives) {
					bool skip = false;
					const Material& material = primitive->material;
					if (renderFlags & RenderFlags::RenderOpaqueNodes) {
						skip = (material.alphaMode != Material::ALPHAMODE_OPAQUE);
					}
					if (renderFlags & RenderFlags::RenderAlphaMaskedNodes) {
						skip = (material.alphaMode != Material::ALPHAMODE_MASK);
					}
					if (renderFlags & RenderFlags::RenderAlphaBlendedNodes) {
						skip = (material.alphaMode != Material::ALPHAMODE_BLEND);
					}
					if (!skip) {
						if (renderFlags & RenderFlags::BindImages) {
							vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, bindImageSet, 1, &material.descriptorSet, 0, nullptr);
						}
						vkCmdDrawIndexed(commandBuffer, primitive->indexCount, 1, primitive->firstIndex, 0, 0);
					}
				}
			}
			for (auto& child : node->children) {
				DrawNode(child, commandBuffer, renderFlags, pipelineLayout, bindImageSet);
			}
		}
		
		// Draw the glTF scene starting at the top-level-nodes
		void VulkanGLTFModel::Draw(VkCommandBuffer commandBuffer, uint32_t renderFlags, VkPipelineLayout pipelineLayout, uint32_t bindImageSet)
		{
			if (!buffersBound) {
				const VkDeviceSize offsets[1] = {0};
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
				vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			}
			for (auto& node : nodes) {
				DrawNode(node, commandBuffer, renderFlags, pipelineLayout, bindImageSet);
			}
		}

		void VulkanGLTFModel::BindBuffers(VkCommandBuffer commandBuffer)
		{
			const VkDeviceSize offsets[1] = {0};
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			buffersBound = true;
		}

		void VulkanGLTFModel::CreateEmptyTexture(VkQueue transferQueue)
		{
			emptyTexture.device = vulkanDevice;
			emptyTexture.width = 1;
			emptyTexture.height = 1;
			emptyTexture.layerCount = 1;
			emptyTexture.mipLevels = 1;

			size_t bufferSize = emptyTexture.width * emptyTexture.height * 4;
			unsigned char* buffer = new unsigned char[bufferSize];
			memset(buffer, 0, bufferSize);

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingMemory;
			VkBufferCreateInfo bufferCreateInfo = vks::initializers::BufferCreateInfo();
			bufferCreateInfo.size = bufferSize;
			// This buffer is used as a transfer source for the buffer copy
			bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			CheckVulkanResult(vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer));

			VkMemoryAllocateInfo memAllocInfo = vks::initializers::MemoryAllocateInfo();
			VkMemoryRequirements memReqs;
			vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, stagingBuffer, &memReqs);
			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			CheckVulkanResult(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
			CheckVulkanResult(vkBindBufferMemory(vulkanDevice->logicalDevice, stagingBuffer, stagingMemory, 0));

			// Copy texture data into staging buffer
			uint8_t* data;
			CheckVulkanResult(vkMapMemory(vulkanDevice->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void**)&data));
			memcpy(data, buffer, bufferSize);
			vkUnmapMemory(vulkanDevice->logicalDevice, stagingMemory);

			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = emptyTexture.width;
			bufferCopyRegion.imageExtent.height = emptyTexture.height;
			bufferCopyRegion.imageExtent.depth = 1;

			// Create optimal tiled target image
			VkImageCreateInfo imageCreateInfo = vks::initializers::ImageCreateInfo();
			imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
			imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			imageCreateInfo.mipLevels = 1;
			imageCreateInfo.arrayLayers = 1;
			imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.extent = { emptyTexture.width, emptyTexture.height, 1 };
			imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			CheckVulkanResult(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &emptyTexture.image));

			vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, emptyTexture.image, &memReqs);
			memAllocInfo.allocationSize = memReqs.size;
			memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			CheckVulkanResult(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &emptyTexture.deviceMemory));
			CheckVulkanResult(vkBindImageMemory(vulkanDevice->logicalDevice, emptyTexture.image, emptyTexture.deviceMemory, 0));

			VkImageSubresourceRange subresourceRange{};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 1;

			VkCommandBuffer copyCmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			vks::utils::SetImageLayout(copyCmd, emptyTexture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
			vkCmdCopyBufferToImage(copyCmd, stagingBuffer, emptyTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);
			vks::utils::SetImageLayout(copyCmd, emptyTexture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
			vulkanDevice->FlushCommandBuffer(copyCmd, transferQueue);
			emptyTexture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// Clean up staging resources
			vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, stagingMemory, nullptr);

			VkSamplerCreateInfo samplerCreateInfo = vks::initializers::SamplerCreateInfo();
			samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
			samplerCreateInfo.maxAnisotropy = 1.0f;
			CheckVulkanResult(vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &emptyTexture.sampler));

			VkImageViewCreateInfo viewCreateInfo = vks::initializers::ImageViewCreateInfo();
			viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			viewCreateInfo.subresourceRange.levelCount = 1;
			viewCreateInfo.image = emptyTexture.image;
			CheckVulkanResult(vkCreateImageView(vulkanDevice->logicalDevice, &viewCreateInfo, nullptr, &emptyTexture.view));

			emptyTexture.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			emptyTexture.descriptor.imageView = emptyTexture.view;
			emptyTexture.descriptor.sampler = emptyTexture.sampler;
		}

		Texture* VulkanGLTFModel::GetTexture(uint32_t index)
		{
			if (index < textures.size())
				return &textures[index];
			
			return nullptr;
		}

		void VulkanGLTFModel::GetSceneDimensions()
		{
			dimensions.min = glm::vec3(FLT_MAX);
			dimensions.max = glm::vec3(-FLT_MAX);
			for (auto node : nodes) {
				GetNodeDimensions(node, dimensions.min, dimensions.max);
			}
			dimensions.size = dimensions.max - dimensions.min;
			dimensions.center = (dimensions.min + dimensions.max) / 2.0f;
			dimensions.radius = glm::distance(dimensions.min, dimensions.max) / 2.0f;
		}

		void VulkanGLTFModel::GetNodeDimensions(Node *node, glm::vec3 &min, glm::vec3 &max)
		{
			if (node->mesh) {
				for (Primitive *primitive : node->mesh->primitives) {
					glm::vec4 locMin = glm::vec4(primitive->dimensions.min, 1.0f) * node->GetMatrix();
					glm::vec4 locMax = glm::vec4(primitive->dimensions.max, 1.0f) * node->GetMatrix();
					if (locMin.x < min.x) { min.x = locMin.x; }
					if (locMin.y < min.y) { min.y = locMin.y; }
					if (locMin.z < min.z) { min.z = locMin.z; }
					if (locMax.x > max.x) { max.x = locMax.x; }
					if (locMax.y > max.y) { max.y = locMax.y; }
					if (locMax.z > max.z) { max.z = locMax.z; }
				}
			}
			for (auto child : node->children) {
				GetNodeDimensions(child, min, max);
			}
		}

		void VulkanGLTFModel::PrepareNodeDescriptor(Node* node, VkDescriptorSetLayout descriptorSetLayout)
		{
			if (node->mesh)
			{
				VkDescriptorSetAllocateInfo descriptorSetAllocInfo{};
				descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				descriptorSetAllocInfo.descriptorPool = descriptorPool;
				descriptorSetAllocInfo.pSetLayouts = &descriptorSetLayout;
				descriptorSetAllocInfo.descriptorSetCount = 1;
				CheckVulkanResult(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &descriptorSetAllocInfo, &node->mesh->uniformBuffer.descriptorSet));

				VkWriteDescriptorSet writeDescriptorSet{};
				writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				writeDescriptorSet.descriptorCount = 1;
				writeDescriptorSet.dstSet = node->mesh->uniformBuffer.descriptorSet;
				writeDescriptorSet.dstBinding = 0;
				writeDescriptorSet.pBufferInfo = &node->mesh->uniformBuffer.descriptor;

				vkUpdateDescriptorSets(vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
			}
			for (auto& child : node->children) {
				PrepareNodeDescriptor(child, descriptorSetLayout);
			}
		}
	};
}
