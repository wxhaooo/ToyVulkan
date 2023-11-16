﻿#include <VulkanInitializers.h>

namespace vks
{
    namespace initializers
    {
#pragma region Sync

    	VkSemaphoreCreateInfo SemaphoreCreateInfo()
        {
        	VkSemaphoreCreateInfo semaphoreCreateInfo {};
        	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        	return semaphoreCreateInfo;
        }

    	VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags)
        {
        	VkFenceCreateInfo fenceCreateInfo {};
        	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        	fenceCreateInfo.flags = flags;
        	return fenceCreateInfo;
        }
    	
#pragma endregion Sync

#pragma region Memory
        
        VkBufferCreateInfo BufferCreateInfo()
        {
            VkBufferCreateInfo bufCreateInfo {};
            bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            return bufCreateInfo;
        }

        VkBufferCreateInfo BufferCreateInfo(VkBufferUsageFlags usage, VkDeviceSize size)
        {
            VkBufferCreateInfo bufCreateInfo {};
            bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufCreateInfo.usage = usage;
            bufCreateInfo.size = size;
            return bufCreateInfo;
        }

        VkMemoryAllocateInfo MemoryAllocateInfo()
        {
            VkMemoryAllocateInfo memAllocInfo {};
            memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            return memAllocInfo;
        }

        VkMappedMemoryRange MappedMemoryRange()
        {
            VkMappedMemoryRange mappedMemoryRange {};
            mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            return mappedMemoryRange;
        }

#pragma endregion Memory

#pragma region RenderPass
    	
    	VkRenderPassBeginInfo RenderPassBeginInfo()
        {
        	VkRenderPassBeginInfo renderPassBeginInfo {};
        	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        	return renderPassBeginInfo;
        }

    	VkRenderPassCreateInfo RenderPassCreateInfo()
        {
        	VkRenderPassCreateInfo renderPassCreateInfo {};
        	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        	return renderPassCreateInfo;
        }
    	
#pragma endregion RenderPass

#pragma region Image
        
        VkImageCreateInfo ImageCreateInfo()
        {
            VkImageCreateInfo imageCreateInfo {};
            imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            return imageCreateInfo;
        }

        VkSamplerCreateInfo SamplerCreateInfo()
        {
            VkSamplerCreateInfo samplerCreateInfo {};
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.maxAnisotropy = 1.0f;
            return samplerCreateInfo;
        }

        VkImageViewCreateInfo ImageViewCreateInfo()
        {
            VkImageViewCreateInfo imageViewCreateInfo {};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            return imageViewCreateInfo;
        }

        /** @brief Initialize an image memory barrier with no image transfer ownership */
        VkImageMemoryBarrier ImageMemoryBarrier()
        {
            VkImageMemoryBarrier imageMemoryBarrier {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            return imageMemoryBarrier;
        }
        
#pragma endregion Image

#pragma region Command

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t bufferCount)
        {
            VkCommandBufferAllocateInfo commandBufferAllocateInfo {};
            commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocateInfo.commandPool = commandPool;
            commandBufferAllocateInfo.level = level;
            commandBufferAllocateInfo.commandBufferCount = bufferCount;
            return commandBufferAllocateInfo;
        }

        VkCommandBufferBeginInfo CommandBufferBeginInfo()
        {
            VkCommandBufferBeginInfo cmdBufferBeginInfo {};
            cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            return cmdBufferBeginInfo;
        }

    	VkSubmitInfo SubmitInfo()
    	{
    		VkSubmitInfo submitInfo {};
    		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    		return submitInfo;
    	}
        
#pragma endregion Commnand

#pragma region Descriptor

        VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(uint32_t poolSizeCount, VkDescriptorPoolSize* pPoolSizes, uint32_t maxSets)
		{
			VkDescriptorPoolCreateInfo descriptorPoolInfo {};
			descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.poolSizeCount = poolSizeCount;
			descriptorPoolInfo.pPoolSizes = pPoolSizes;
			descriptorPoolInfo.maxSets = maxSets;
			return descriptorPoolInfo;
		}

		VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo(const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets)
		{
			VkDescriptorPoolCreateInfo descriptorPoolInfo{};
			descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
			descriptorPoolInfo.pPoolSizes = poolSizes.data();
			descriptorPoolInfo.maxSets = maxSets;
			return descriptorPoolInfo;
		}

		VkDescriptorPoolSize DescriptorPoolSize(VkDescriptorType type, uint32_t descriptorCount)
		{
			VkDescriptorPoolSize descriptorPoolSize {};
			descriptorPoolSize.type = type;
			descriptorPoolSize.descriptorCount = descriptorCount;
			return descriptorPoolSize;
		}

		VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding, uint32_t descriptorCount)
		{
			VkDescriptorSetLayoutBinding setLayoutBinding {};
			setLayoutBinding.descriptorType = type;
			setLayoutBinding.stageFlags = stageFlags;
			setLayoutBinding.binding = binding;
			setLayoutBinding.descriptorCount = descriptorCount;
			return setLayoutBinding;
		}

		VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(const VkDescriptorSetLayoutBinding* pBindings, uint32_t bindingCount)
		{
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo {};
			descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCreateInfo.pBindings = pBindings;
			descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
			return descriptorSetLayoutCreateInfo;
		}

		VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo(const std::vector<VkDescriptorSetLayoutBinding>& bindings)
		{
			VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
			descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptorSetLayoutCreateInfo.pBindings = bindings.data();
			descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
			return descriptorSetLayoutCreateInfo;
		}

    	VkDescriptorSetAllocateInfo DescriptorSetAllocateInfo(VkDescriptorPool descriptorPool,const VkDescriptorSetLayout* pSetLayouts,uint32_t descriptorSetCount)
		{
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo {};
			descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptorSetAllocateInfo.descriptorPool = descriptorPool;
			descriptorSetAllocateInfo.pSetLayouts = pSetLayouts;
			descriptorSetAllocateInfo.descriptorSetCount = descriptorSetCount;
			return descriptorSetAllocateInfo;
		}

		VkDescriptorImageInfo DescriptorImageInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
		{
			VkDescriptorImageInfo descriptorImageInfo {};
			descriptorImageInfo.sampler = sampler;
			descriptorImageInfo.imageView = imageView;
			descriptorImageInfo.imageLayout = imageLayout;
			return descriptorImageInfo;
		}

		VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet,VkDescriptorType type,uint32_t binding,VkDescriptorBufferInfo* bufferInfo,uint32_t descriptorCount)
		{
			VkWriteDescriptorSet writeDescriptorSet {};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = dstSet;
			writeDescriptorSet.descriptorType = type;
			writeDescriptorSet.dstBinding = binding;
			writeDescriptorSet.pBufferInfo = bufferInfo;
			writeDescriptorSet.descriptorCount = descriptorCount;
			return writeDescriptorSet;
		}

		VkWriteDescriptorSet WriteDescriptorSet(VkDescriptorSet dstSet,VkDescriptorType type,uint32_t binding,VkDescriptorImageInfo *imageInfo,uint32_t descriptorCount)
		{
			VkWriteDescriptorSet writeDescriptorSet {};
			writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescriptorSet.dstSet = dstSet;
			writeDescriptorSet.descriptorType = type;
			writeDescriptorSet.dstBinding = binding;
			writeDescriptorSet.pImageInfo = imageInfo;
			writeDescriptorSet.descriptorCount = descriptorCount;
			return writeDescriptorSet;
		}

    	VkPushConstantRange PushConstantRange(VkShaderStageFlags stageFlags,uint32_t size,uint32_t offset)
        {
        	VkPushConstantRange pushConstantRange {};
        	pushConstantRange.stageFlags = stageFlags;
        	pushConstantRange.offset = offset;
        	pushConstantRange.size = size;
        	return pushConstantRange;
        }
        
#pragma endregion Descriptor

#pragma region Binding

    	VkVertexInputBindingDescription VertexInputBindingDescription(uint32_t binding,uint32_t stride,VkVertexInputRate inputRate)
        {
        	VkVertexInputBindingDescription vInputBindDescription {};
        	vInputBindDescription.binding = binding;
        	vInputBindDescription.stride = stride;
        	vInputBindDescription.inputRate = inputRate;
        	return vInputBindDescription;
        }

    	VkVertexInputAttributeDescription VertexInputAttributeDescription(uint32_t binding,uint32_t location,VkFormat format,uint32_t offset)

        {
        	VkVertexInputAttributeDescription vInputAttribDescription {};
        	vInputAttribDescription.location = location;
        	vInputAttribDescription.binding = binding;
        	vInputAttribDescription.format = format;
        	vInputAttribDescription.offset = offset;
        	return vInputAttribDescription;
        }
    	
#pragma endregion Binding

#pragma region Pipeline

    	VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(const VkDescriptorSetLayout* pSetLayouts, uint32_t setLayoutCount)
        {
        	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
        	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        	pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
        	pipelineLayoutCreateInfo.pSetLayouts = pSetLayouts;
        	return pipelineLayoutCreateInfo;
        }

    	VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo(uint32_t setLayoutCount)
        {
        	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        	pipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
        	return pipelineLayoutCreateInfo;
        }

    	VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo()
		{
			VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo {};
			pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			return pipelineVertexInputStateCreateInfo;
		}

		VkPipelineVertexInputStateCreateInfo PipelineVertexInputStateCreateInfo(const std::vector<VkVertexInputBindingDescription> &vertexBindingDescriptions, const std::vector<VkVertexInputAttributeDescription> &vertexAttributeDescriptions
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

		VkPipelineInputAssemblyStateCreateInfo PipelineInputAssemblyStateCreateInfo(VkPrimitiveTopology topology,VkPipelineInputAssemblyStateCreateFlags flags,VkBool32 primitiveRestartEnable)
		{
			VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo {};
			pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			pipelineInputAssemblyStateCreateInfo.topology = topology;
			pipelineInputAssemblyStateCreateInfo.flags = flags;
			pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = primitiveRestartEnable;
			return pipelineInputAssemblyStateCreateInfo;
		}

		VkPipelineRasterizationStateCreateInfo PipelineRasterizationStateCreateInfo(VkPolygonMode polygonMode,VkCullModeFlags cullMode,VkFrontFace frontFace,VkPipelineRasterizationStateCreateFlags flags)
		{
			VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo {};
			pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			pipelineRasterizationStateCreateInfo.polygonMode = polygonMode;
			pipelineRasterizationStateCreateInfo.cullMode = cullMode;
			pipelineRasterizationStateCreateInfo.frontFace = frontFace;
			pipelineRasterizationStateCreateInfo.flags = flags;
			pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
			pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
			return pipelineRasterizationStateCreateInfo;
		}

		VkPipelineColorBlendAttachmentState PipelineColorBlendAttachmentState(VkColorComponentFlags colorWriteMask, VkBool32 blendEnable)
		{
			VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState {};
			pipelineColorBlendAttachmentState.colorWriteMask = colorWriteMask;
			pipelineColorBlendAttachmentState.blendEnable = blendEnable;
			return pipelineColorBlendAttachmentState;
		}

		VkPipelineColorBlendStateCreateInfo PipelineColorBlendStateCreateInfo(uint32_t attachmentCount, const VkPipelineColorBlendAttachmentState * pAttachments)
		{
			VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo {};
			pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			pipelineColorBlendStateCreateInfo.attachmentCount = attachmentCount;
			pipelineColorBlendStateCreateInfo.pAttachments = pAttachments;
			return pipelineColorBlendStateCreateInfo;
		}

		VkPipelineDepthStencilStateCreateInfo PipelineDepthStencilStateCreateInfo(VkBool32 depthTestEnable,VkBool32 depthWriteEnable,VkCompareOp depthCompareOp)
		{
			VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo {};
			pipelineDepthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			pipelineDepthStencilStateCreateInfo.depthTestEnable = depthTestEnable;
			pipelineDepthStencilStateCreateInfo.depthWriteEnable = depthWriteEnable;
			pipelineDepthStencilStateCreateInfo.depthCompareOp = depthCompareOp;
			pipelineDepthStencilStateCreateInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;
			return pipelineDepthStencilStateCreateInfo;
		}

		VkPipelineViewportStateCreateInfo PipelineViewportStateCreateInfo(uint32_t viewportCount,uint32_t scissorCount,VkPipelineViewportStateCreateFlags flags)
		{
			VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo {};
			pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			pipelineViewportStateCreateInfo.viewportCount = viewportCount;
			pipelineViewportStateCreateInfo.scissorCount = scissorCount;
			pipelineViewportStateCreateInfo.flags = flags;
			return pipelineViewportStateCreateInfo;
		}

		VkPipelineMultisampleStateCreateInfo PipelineMultisampleStateCreateInfo(VkSampleCountFlagBits rasterizationSamples,VkPipelineMultisampleStateCreateFlags flags)
		{
			VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo {};
			pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			pipelineMultisampleStateCreateInfo.rasterizationSamples = rasterizationSamples;
			pipelineMultisampleStateCreateInfo.flags = flags;
			return pipelineMultisampleStateCreateInfo;
		}

		VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(const VkDynamicState * pDynamicStates,uint32_t dynamicStateCount,VkPipelineDynamicStateCreateFlags flags)
		{
			VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo {};
			pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates;
			pipelineDynamicStateCreateInfo.dynamicStateCount = dynamicStateCount;
			pipelineDynamicStateCreateInfo.flags = flags;
			return pipelineDynamicStateCreateInfo;
		}

		VkPipelineDynamicStateCreateInfo PipelineDynamicStateCreateInfo(const std::vector<VkDynamicState>& pDynamicStates,VkPipelineDynamicStateCreateFlags flags)
		{
			VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{};
			pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			pipelineDynamicStateCreateInfo.pDynamicStates = pDynamicStates.data();
			pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(pDynamicStates.size());
			pipelineDynamicStateCreateInfo.flags = flags;
			return pipelineDynamicStateCreateInfo;
		}

		VkPipelineTessellationStateCreateInfo PipelineTessellationStateCreateInfo(uint32_t patchControlPoints)
		{
			VkPipelineTessellationStateCreateInfo pipelineTessellationStateCreateInfo {};
			pipelineTessellationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
			pipelineTessellationStateCreateInfo.patchControlPoints = patchControlPoints;
			return pipelineTessellationStateCreateInfo;
		}

		VkGraphicsPipelineCreateInfo PipelineCreateInfo(VkPipelineLayout layout,VkRenderPass renderPass,VkPipelineCreateFlags flags)
		{
			VkGraphicsPipelineCreateInfo pipelineCreateInfo {};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.layout = layout;
			pipelineCreateInfo.renderPass = renderPass;
			pipelineCreateInfo.flags = flags;
			pipelineCreateInfo.basePipelineIndex = -1;
			pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			return pipelineCreateInfo;
		}

		VkGraphicsPipelineCreateInfo PipelineCreateInfo()
		{
			VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
			pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineCreateInfo.basePipelineIndex = -1;
			pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
			return pipelineCreateInfo;
		}

    	VkPipelineCacheCreateInfo PipelineCacheCreateInfo()
    	{
    		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    		return pipelineCacheCreateInfo;
    	}

    	VkComputePipelineCreateInfo ComputePipelineCreateInfo(VkPipelineLayout layout, VkPipelineCreateFlags flags)
        {
        	VkComputePipelineCreateInfo computePipelineCreateInfo {};
        	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        	computePipelineCreateInfo.layout = layout;
        	computePipelineCreateInfo.flags = flags;
        	return computePipelineCreateInfo;
        }

    	VkViewport Viewport(float width, float height, float minDepth, float maxDepth)
    	{
    		VkViewport viewport {};
    		viewport.width = width;
    		viewport.height = height;
    		viewport.minDepth = minDepth;
    		viewport.maxDepth = maxDepth;
    		return viewport;
    	}

    	VkRect2D Rect2D(uint32_t width, uint32_t height, int32_t offsetX, int32_t offsetY)
    	{
    		VkRect2D rect2D {};
    		rect2D.extent.width = width;
    		rect2D.extent.height = height;
    		rect2D.offset.x = offsetX;
    		rect2D.offset.y = offsetY;
    		return rect2D;
    	}
    	
#pragma endregion Pipeline
        
    }    
}
