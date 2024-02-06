#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <VulkanUtils.h>
#include <vulkan/vulkan_core.h>
#include <VulkanInitializers.h>
#include <VulkanHelper.h>

#include "VulkanFrameBuffer.h"

namespace vks {
    namespace utils {
        VkBool32 GetSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat) {
            // Since all depth formats may be optional, we need to find a suitable depth format to use
            // Start with the highest precision packed format
            std::vector<VkFormat> formatList = {
                    VK_FORMAT_D32_SFLOAT_S8_UINT,
                    VK_FORMAT_D32_SFLOAT,
                    VK_FORMAT_D24_UNORM_S8_UINT,
                    VK_FORMAT_D16_UNORM_S8_UINT,
                    VK_FORMAT_D16_UNORM
            };

            for (auto &format: formatList) {
                VkFormatProperties formatProps;
                vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
                if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                    *depthFormat = format;
                    return true;
                }
            }

            return false;
        }

        VkBool32 GetSupportedDepthStencilFormat(VkPhysicalDevice physicalDevice, VkFormat *depthStencilFormat) {
            std::vector<VkFormat> formatList = {
                    VK_FORMAT_D32_SFLOAT_S8_UINT,
                    VK_FORMAT_D24_UNORM_S8_UINT,
                    VK_FORMAT_D16_UNORM_S8_UINT,
            };

            for (auto &format: formatList) {
                VkFormatProperties formatProps;
                vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
                if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                    *depthStencilFormat = format;
                    return true;
                }
            }

            return false;
        }

        // Create an image memory barrier for changing the layout of
        // an image and put it into an active command buffer
        // See chapter 11.4 "Image Layout" for details
        void SetImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout,
                            VkImageLayout newImageLayout,
                            VkImageSubresourceRange subresourceRange, VkPipelineStageFlags srcStageMask,
                            VkPipelineStageFlags dstStageMask) {
            // Create an image barrier object
            VkImageMemoryBarrier imageMemoryBarrier = vks::initializers::ImageMemoryBarrier();
            imageMemoryBarrier.oldLayout = oldImageLayout;
            imageMemoryBarrier.newLayout = newImageLayout;
            imageMemoryBarrier.image = image;
            imageMemoryBarrier.subresourceRange = subresourceRange;

            // Source layouts (old)
            // Source access mask controls actions that have to be finished on the old layout
            // before it will be transitioned to the new layout
            switch (oldImageLayout) {
                case VK_IMAGE_LAYOUT_UNDEFINED:
                    // Image layout is undefined (or does not matter)
                    // Only valid as initial layout
                    // No flags required, listed only for completeness
                    imageMemoryBarrier.srcAccessMask = 0;
                    break;

                case VK_IMAGE_LAYOUT_PREINITIALIZED:
                    // Image is preinitialized
                    // Only valid as initial layout for linear images, preserves memory contents
                    // Make sure host writes have been finished
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                    break;

                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    // Image is a color attachment
                    // Make sure any writes to the color buffer have been finished
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    break;

                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                    // Image is a depth/stencil attachment
                    // Make sure any writes to the depth/stencil buffer have been finished
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    break;

                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                    // Image is a transfer source
                    // Make sure any reads from the image have been finished
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    break;

                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    // Image is a transfer destination
                    // Make sure any writes to the image have been finished
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    break;

                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    // Image is read by a shader
                    // Make sure any shader reads from the image have been finished
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    break;
                default:
                    // Other source layouts aren't handled (yet)
                    break;
            }

            // Target layouts (new)
            // Destination access mask controls the dependency for the new image layout
            switch (newImageLayout) {
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    // Image will be used as a transfer destination
                    // Make sure any writes to the image have been finished
                    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    break;

                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                    // Image will be used as a transfer source
                    // Make sure any reads from the image have been finished
                    imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                    break;

                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    // Image will be used as a color attachment
                    // Make sure any writes to the color buffer have been finished
                    imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    break;

                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                    // Image layout will be used as a depth/stencil attachment
                    // Make sure any writes to depth/stencil buffer have been finished
                    imageMemoryBarrier.dstAccessMask =
                            imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    break;

                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    // Image will be read in a shader (sampler, input attachment)
                    // Make sure any writes to the image have been finished
                    if (imageMemoryBarrier.srcAccessMask == 0) {
                        imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                    }
                    imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    break;
                default:
                    // Other source layouts aren't handled (yet)
                    break;
            }

            // Put barrier inside setup command buffer
            vkCmdPipelineBarrier(
                    cmdbuffer,
                    srcStageMask,
                    dstStageMask,
                    0,
                    0, nullptr,
                    0, nullptr,
                    1, &imageMemoryBarrier);
        }

        // Fixed sub resource on first mip level and layer
        void SetImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask,
                            VkImageLayout oldImageLayout,
                            VkImageLayout newImageLayout, VkPipelineStageFlags srcStageMask,
                            VkPipelineStageFlags dstStageMask) {
            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask = aspectMask;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
            subresourceRange.layerCount = 1;
            SetImageLayout(cmdbuffer, image, oldImageLayout, newImageLayout, subresourceRange, srcStageMask,
                           dstStageMask);
        }

        VkShaderModule LoadShader(const char *fileName, VkDevice device) {
            std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

            if (is.is_open()) {
                size_t size = is.tellg();
                is.seekg(0, std::ios::beg);
                char *shaderCode = new char[size];
                is.read(shaderCode, size);
                is.close();

                assert(size > 0);

                VkShaderModule shaderModule;
                VkShaderModuleCreateInfo moduleCreateInfo{};
                moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
                moduleCreateInfo.codeSize = size;
                moduleCreateInfo.pCode = (uint32_t *) shaderCode;

                CheckVulkanResult(vkCreateShaderModule(device, &moduleCreateInfo, NULL, &shaderModule));

                delete[] shaderCode;

                return shaderModule;
            } else {
                std::cerr << "Error: Could not open shader file \"" << fileName << "\"" << "\n";
                return VK_NULL_HANDLE;
            }
        }

        bool HasDepth(VkFormat format) {
            std::vector<VkFormat> formats =
                    {
                            VK_FORMAT_D16_UNORM,
                            VK_FORMAT_X8_D24_UNORM_PACK32,
                            VK_FORMAT_D32_SFLOAT,
                            VK_FORMAT_D16_UNORM_S8_UINT,
                            VK_FORMAT_D24_UNORM_S8_UINT,
                            VK_FORMAT_D32_SFLOAT_S8_UINT,
                    };
            return std::find(formats.begin(), formats.end(), format) != std::end(formats);
        }

        bool HasStencil(VkFormat format) {
            std::vector<VkFormat> formats =
                    {
                            VK_FORMAT_S8_UINT,
                            VK_FORMAT_D16_UNORM_S8_UINT,
                            VK_FORMAT_D24_UNORM_S8_UINT,
                            VK_FORMAT_D32_SFLOAT_S8_UINT,
                    };
            return std::find(formats.begin(), formats.end(), format) != std::end(formats);
        }

        bool IsDepthStencil(VkFormat format) {
            return (HasDepth(format) || HasStencil(format));
        }

        Texture2D* CreateDefaultTexture2D(VulkanDevice *vulkanDevice, VkQueue transferQueue, uint32_t width, uint32_t height,
                                              glm::vec4 clearColor) {

            Texture2D* texture2D = new Texture2D();
            texture2D->device = vulkanDevice;
            texture2D->width = width;
            texture2D->height = height;
            texture2D->layerCount = 1;
            texture2D->mipLevels = 1;

            size_t bufferSize = texture2D->width * texture2D->height * 4;
            unsigned char *buffer = new unsigned char[bufferSize];
            for (int i = 0; i < bufferSize; i += 4) {
                buffer[i] = static_cast<char>(clearColor[0]);
                buffer[i + 1] = static_cast<char>(clearColor[1]);
                buffer[i + 2] = static_cast<char>(clearColor[2]);
                buffer[i + 3] = static_cast<char>(clearColor[3]);
            }
//            memset(buffer, 0, bufferSize);

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
            memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits,
                                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            CheckVulkanResult(vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory));
            CheckVulkanResult(vkBindBufferMemory(vulkanDevice->logicalDevice, stagingBuffer, stagingMemory, 0));

            // Copy texture data into staging buffer
            uint8_t *data;
            CheckVulkanResult(
                    vkMapMemory(vulkanDevice->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **) &data));
            memcpy(data, buffer, bufferSize);
            vkUnmapMemory(vulkanDevice->logicalDevice, stagingMemory);

            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = texture2D->width;
            bufferCopyRegion.imageExtent.height = texture2D->height;
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
            imageCreateInfo.extent = {texture2D->width, texture2D->height, 1};
            imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            CheckVulkanResult(vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &texture2D->image));

            vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, texture2D->image, &memReqs);
            memAllocInfo.allocationSize = memReqs.size;
            memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits,
                                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            CheckVulkanResult(
                    vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &texture2D->deviceMemory));
            CheckVulkanResult(
                    vkBindImageMemory(vulkanDevice->logicalDevice, texture2D->image, texture2D->deviceMemory, 0));

            VkImageSubresourceRange subresourceRange{};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel = 0;
            subresourceRange.levelCount = 1;
            subresourceRange.layerCount = 1;

            VkCommandBuffer copyCmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
            vks::utils::SetImageLayout(copyCmd, texture2D->image, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);
            vkCmdCopyBufferToImage(copyCmd, stagingBuffer, texture2D->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                   &bufferCopyRegion);
            vks::utils::SetImageLayout(copyCmd, texture2D->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange);
            vulkanDevice->FlushCommandBuffer(copyCmd, transferQueue);
            texture2D->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

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
            CheckVulkanResult(
                    vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &texture2D->sampler));

            VkImageViewCreateInfo viewCreateInfo = vks::initializers::ImageViewCreateInfo();
            viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            viewCreateInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            viewCreateInfo.subresourceRange.levelCount = 1;
            viewCreateInfo.image = texture2D->image;
            CheckVulkanResult(
                    vkCreateImageView(vulkanDevice->logicalDevice, &viewCreateInfo, nullptr, &texture2D->view));

            texture2D->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            texture2D->descriptor.imageView = texture2D->view;
            texture2D->descriptor.sampler = texture2D->sampler;

            return texture2D;
        }

        VkSampler CreateSampler(const VulkanDevice* device, const VulkanSamplerCreateInfo& samplerCreateInfo)
        {
            VkSampler sampler;
            VkSamplerCreateInfo samplerInfo = vks::initializers::SamplerCreateInfo();
            samplerInfo.magFilter = samplerCreateInfo.magFiler;
            samplerInfo.minFilter = samplerCreateInfo.minFiler;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU = samplerCreateInfo.addressMode;
            samplerInfo.addressModeV = samplerCreateInfo.addressMode;
            samplerInfo.addressModeW = samplerCreateInfo.addressMode;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 1.0f;
            samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            CheckVulkanResult(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &sampler));
            return sampler;
        }
    }
}
