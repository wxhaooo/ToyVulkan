#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vks
{
    namespace initializers
    {
        VkSubmitInfo SubmitInfo();
        VkSemaphoreCreateInfo SemaphoreCreateInfo();
        VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0);

        VkBufferCreateInfo BufferCreateInfo();
        VkBufferCreateInfo BufferCreateInfo(VkBufferUsageFlags usage,VkDeviceSize size);
        VkMemoryAllocateInfo MemoryAllocateInfo();
        VkMappedMemoryRange MappedMemoryRange();

#pragma region Image
        
        VkImageCreateInfo ImageCreateInfo();
        VkSamplerCreateInfo SamplerCreateInfo();
        VkImageViewCreateInfo ImageViewCreateInfo();
        /** @brief Initialize an image memory barrier with no image transfer ownership */
        VkImageMemoryBarrier ImageMemoryBarrier();
        
#pragma endregion Image

#pragma region CommandBuffer
        
        VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t bufferCount);
        VkCommandBufferBeginInfo CommandBufferBeginInfo();
        
#pragma endregion CommandBuffer

#pragma region Descriptor

        VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes, uint32_t maxSets);
        VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
        VkDescriptorPoolSize DescriptorPoolSize(VkDescriptorType type,uint32_t descriptorCount);
        VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType type,VkShaderStageFlags stageFlags,uint32_t binding,uint32_t descriptorCount = 1);
        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding* pBindings,uint32_t bindingCount);
        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(const std::vector<VkDescriptorSetLayoutBinding>& bindings);
        VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(VkDescriptorPool descriptorPool,const VkDescriptorSetLayout* pSetLayouts,uint32_t descriptorSetCount);
        VkDescriptorImageInfo DescriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
        VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet,VkDescriptorType type,uint32_t binding,VkDescriptorBufferInfo* bufferInfo,uint32_t descriptorCount = 1);
        VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet,VkDescriptorType type,uint32_t binding,VkDescriptorImageInfo *imageInfo,uint32_t descriptorCount = 1);
        VkPushConstantRange PushConstantRange(VkShaderStageFlags stageFlags,uint32_t size,uint32_t offset);
        
#pragma endregion Descriptor

#pragma region Binding

    	VkVertexInputBindingDescription VertexInputBindingDescription(uint32_t binding,uint32_t stride,VkVertexInputRate inputRate);
    	VkVertexInputAttributeDescription VertexInputAttributeDescription(uint32_t binding,uint32_t location,VkFormat format,uint32_t offset);
    	
#pragma endregion Binding

#pragma region Pipeline

        VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(const VkDescriptorSetLayout* pSetLayouts,uint32_t setLayoutCount = 1);
        VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(uint32_t setLayoutCount = 1);
        VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo();
		VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo(const std::vector<VkVertexInputBindingDescription> &vertexBindingDescriptions, const std::vector<VkVertexInputAttributeDescription> &vertexAttributeDescriptions);
		VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology,VkPipelineInputAssemblyStateCreateFlags flags,VkBool32 primitiveRestartEnable);
		VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode,VkCullModeFlags cullMode,VkFrontFace frontFace,VkPipelineRasterizationStateCreateFlags flags = 0);
		VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState(VkColorComponentFlags colorWriteMask, VkBool32 blendEnable);
		VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(uint32_t attachmentCount, const VkPipelineColorBlendAttachmentState * pAttachments);
		VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilStateCreateInfo(VkBool32 depthTestEnable,VkBool32 depthWriteEnable,VkCompareOp depthCompareOp);
		VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo(uint32_t viewportCount,uint32_t scissorCount,VkPipelineViewportStateCreateFlags flags = 0);
		VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo(VkSampleCountFlagBits rasterizationSamples,VkPipelineMultisampleStateCreateFlags flags = 0);
		VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(const VkDynamicState * pDynamicStates,uint32_t dynamicStateCount,VkPipelineDynamicStateCreateFlags flags = 0);
		VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(const std::vector<VkDynamicState>& pDynamicStates,VkPipelineDynamicStateCreateFlags flags = 0);
		VkPipelineTessellationStateCreateInfo PipelineTessellationStateCreateInfo(uint32_t patchControlPoints);
		VkGraphicsPipelineCreateInfo PipelineCreateInfo(VkPipelineLayout layout,VkRenderPass renderPass,VkPipelineCreateFlags flags = 0);
		VkGraphicsPipelineCreateInfo PipelineCreateInfo();
    	VkComputePipelineCreateInfo ComputePipelineCreateInfo(VkPipelineLayout layout, VkPipelineCreateFlags flags = 0);

#pragma endregion Pipeline
    }    
}
