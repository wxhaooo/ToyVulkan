#include <VulkanHelper.h>
#include <VulkanRenderPass.h>
#include <array>
#include <cassert>
#include <numeric>

#include "VulkanFrameBuffer.h"

namespace vks
{
    OffscreenPass::~OffscreenPass()
    {
        if (renderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(device, renderPass, nullptr);

        DestroyResource();
    }

    void OffscreenPass::DestroyResource()
    {
        {
            for (uint32_t i = 0; i < frameBuffer.size(); i++)
            {
                vkDestroySampler(device, sampler[i], nullptr);
                vkDestroyFramebuffer(device, frameBuffer[i], nullptr);

                vkDestroyImage(device, color[i].image, nullptr);
                vkDestroyImageView(device, color[i].view, nullptr);
                vkFreeMemory(device, color[i].memory, nullptr);

                vkDestroyImage(device, depth[i].image, nullptr);
                vkDestroyImageView(device, depth[i].view, nullptr);
                vkFreeMemory(device, depth[i].memory, nullptr);
            }

            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        }
    }
} // namespace vks

namespace vks
{
    VulkanAttachmentDescription::VulkanAttachmentDescription(const AttachmentCreateInfo& attachmentCreateInfo)
    {
        name = attachmentCreateInfo.name;
        binding = attachmentCreateInfo.binding;
        format = attachmentCreateInfo.format;
        width = attachmentCreateInfo.width;
        height = attachmentCreateInfo.height;
        layerCount = attachmentCreateInfo.layerCount;
        usage = attachmentCreateInfo.usage;
        imageSampleCount = attachmentCreateInfo.imageSampleCount;

        // Fill attachment description
        description = {};
        description.samples = imageSampleCount;
        description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        description.storeOp = (usage & VK_IMAGE_USAGE_SAMPLED_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        description.format = format;
        description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // Final layout
        // If not, final layout depends on attachment type
//        if(attachmentCreateInfo.finalLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            if (utils::HasDepth(format) || utils::HasStencil(format))
                description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            else
                description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
//        else
//            description.finalLayout = attachmentCreateInfo.finalLayout;
    }
    
    VulkanSubPass::VulkanSubPass(const std::string& subPassName,
                                 VkPipelineBindPoint subPassBindPoint,
                                 vks::VulkanDevice* device)
        : name(subPassName), bindPoint(subPassBindPoint), vulkanDevice(device)
    {
    }

    void VulkanSubPass::AddAttachments(const std::vector<VulkanAttachmentDescription*>& attachmentDescriptions,
                                       const std::vector<uint32_t>& colorAttachmentIndices,
                                       const std::vector<uint32_t>& inputAttachmentIndices)
    {
        // Collect attachment references
        for (uint32_t i = 0; i < colorAttachmentIndices.size(); i++)
        {
            uint32_t attachmentIndex = colorAttachmentIndices[i];
            if (attachmentIndex >= attachmentDescriptions.size())
                continue;
            VulkanAttachmentDescription* attachment = attachmentDescriptions[attachmentIndex];
            if (vks::utils::IsDepthStencil(attachment->format))
            {
                // Only one depth attachment allowed
                assert(!hasDepth);
                depthReference.attachment = attachmentIndex;
                depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                hasDepth = true;
            }
            else
            {
                colorReferences.push_back(
                    {attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
                hasColor = true;
            }
        }

        for (uint32_t i = 0; i < inputAttachmentIndices.size(); i++)
        {
            uint32_t attachmentIndex = inputAttachmentIndices[i];
            if (attachmentIndex >= attachmentDescriptions.size())
                continue;
            VulkanAttachmentDescription* attachment = attachmentDescriptions[attachmentIndex];
            if (vks::utils::IsDepthStencil(attachment->format)) continue;

            inputReferences.push_back(
                    {attachmentIndex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
            hasInput = true;
        }
    }

    void VulkanSubPass::CreateDescription()
    {
        description = {};
        // Default render pass setup uses only one subPass
        description.pipelineBindPoint = bindPoint;
        if (hasColor)
        {
            description.pColorAttachments = colorReferences.data();
            description.colorAttachmentCount =
                static_cast<uint32_t>(colorReferences.size());
        }

        if (hasInput)
        {
            description.pInputAttachments = inputReferences.data();
            description.inputAttachmentCount =
                    static_cast<uint32_t>(inputReferences.size());
        }

        if (hasDepth)
            description.pDepthStencilAttachment = &depthReference;
    }

    VkSubpassDescription& VulkanSubPass::GetDescription()
    {
        return description;
    }

    VulkanRenderPass::VulkanRenderPass(const std::string& renderPassName,
                                       VulkanDevice* device)
        : name(renderPassName), vulkanDevice(device)
    {
        attachmentCount = 0;
        colorAttachmentCount = 0;
    }

    VulkanRenderPass::~VulkanRenderPass() { Destroy(); }

    void VulkanRenderPass::Destroy()
    {
        // release subpass resource
        subPassArray.clear();
        subPassName2InstMap.clear();

        vkDestroyRenderPass(vulkanDevice->logicalDevice, renderPass, nullptr);
    }

    void VulkanRenderPass::AddSubPass(
        const std::string &subPassName,
        VkPipelineBindPoint subPassBindPoint,
        const std::vector<std::string>& colorAttachmentNames,
        const std::vector<std::string>& inputAttachmentNames)
    {
        std::vector<uint32_t> colorAttachmentIndices;
        std::vector<uint32_t> inputAttachmentIndices;

        colorAttachmentIndices.resize(colorAttachmentNames.size());
        inputAttachmentIndices.resize(inputAttachmentNames.size());

        for (int i = 0; i < colorAttachmentNames.size(); i++)
        {
            std::string colorAttachmentName = colorAttachmentNames[i];
            auto attachment = std::find_if(attachmentDescriptions.begin(), attachmentDescriptions.end(),
                                           [&](const VulkanAttachmentDescription* const attachmentDescription)
                                           {
                                               return colorAttachmentName == attachmentDescription->name;
                                           });

            if (attachment == attachmentDescriptions.end())
            {
                vks::helper::ExitFatal("do not find target attachment name in framebuffer", -1);
                return;
            }

            colorAttachmentIndices[i] = (*attachment)->binding;
        }

        for (int i = 0; i < inputAttachmentNames.size(); i++)
        {
            std::string inputAttachmentName = inputAttachmentNames[i];
            auto attachment = std::find_if(attachmentDescriptions.begin(), attachmentDescriptions.end(),
                                           [&](const VulkanAttachmentDescription* const attachmentDescription)
                                           {
                                               return inputAttachmentName == attachmentDescription->name;
                                           });

            if (attachment == attachmentDescriptions.end())
            {
                vks::helper::ExitFatal("do not find target attachment name in framebuffer", -1);
                return;
            }

            inputAttachmentIndices[i] = (*attachment)->binding;
        }

        AddSubPass(subPassName, subPassBindPoint, colorAttachmentIndices, inputAttachmentIndices);
    }

    void VulkanRenderPass::AddSubPass(
        const std::string& subPassName, VkPipelineBindPoint subPassBindPoint,
        const std::vector<uint32_t> &colorAttachmentIndices,
        const std::vector<uint32_t> &inputAttachmentIndices)
    {
        if (subPassName2InstMap.count(subPassName))
            return;

        VulkanSubPass* newSubPassInst =
            new VulkanSubPass(subPassName, subPassBindPoint, vulkanDevice);
        newSubPassInst->AddAttachments(attachmentDescriptions, colorAttachmentIndices, inputAttachmentIndices);
        newSubPassInst->CreateDescription();

        // add subPass
        subPassArray.push_back(newSubPassInst);
        subPassName2InstMap.emplace(subPassName, newSubPassInst);
    }

    void VulkanRenderPass::AddSubPassDependency(std::vector<VkSubpassDependency> newSubPassDependencies)
    {
        for (auto subPassDependency : newSubPassDependencies)
            subPassDependencies.push_back(subPassDependency);
    }

    void VulkanRenderPass::CreateCustomRenderPass()
    {
        uint32_t attachmentCount = AttachmentCount();
        std::vector<VkAttachmentDescription> allAttachmentDescriptions(attachmentCount);
        for (uint32_t i = 0; i < attachmentCount; i++)
            allAttachmentDescriptions[i] = attachmentDescriptions[i]->description;

        std::vector<VkSubpassDescription> subPasses;
        for (auto& subPass : subPassArray)
        {
            VkSubpassDescription subPassDescription = subPass->GetDescription();
            subPasses.push_back(subPassDescription);
        }

        // Create render pass
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.pAttachments = allAttachmentDescriptions.data();
        renderPassInfo.attachmentCount = static_cast<uint32_t>(allAttachmentDescriptions.size());
        renderPassInfo.subpassCount = subPasses.size();
        renderPassInfo.pSubpasses = subPasses.data();
        renderPassInfo.dependencyCount = subPassDependencies.size();
        renderPassInfo.pDependencies = subPassDependencies.data();
        CheckVulkanResult(vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &renderPass));
    }

    void VulkanRenderPass::Init(bool isDefault)
    {
        if(isDefault)
            CreateDefaultRenderPass();
        else
            CreateCustomRenderPass();
    }

    void VulkanRenderPass::CreateDefaultRenderPass()
    {
        // subpass
        int attachmentCount = attachmentDescriptions.size();
        std::vector<uint32_t> subPassAttachmentIndices(attachmentCount);
        std::iota(subPassAttachmentIndices.begin(),subPassAttachmentIndices.end(),0);
        AddSubPass("default", VK_PIPELINE_BIND_POINT_GRAPHICS, subPassAttachmentIndices,{});
        AddSubPassDependency(
            {
                {
                    VK_SUBPASS_EXTERNAL, 0,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_ACCESS_MEMORY_READ_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_DEPENDENCY_BY_REGION_BIT,
                },
                {
                    0,VK_SUBPASS_EXTERNAL,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_ACCESS_MEMORY_READ_BIT,
                    VK_DEPENDENCY_BY_REGION_BIT,
                }
            });

        CreateCustomRenderPass();
    }

    int VulkanRenderPass::AttachmentCount() { return attachmentCount; }

    int VulkanRenderPass::ColorAttachmentCount() { return colorAttachmentCount; }

    void VulkanRenderPass::AddAttachment(
        const vks::AttachmentCreateInfo& createInfo)
    {
        VulkanAttachmentDescription* attachmentDescription = new VulkanAttachmentDescription(createInfo);
        attachmentName2DescriptionMap.emplace(attachmentDescription->name, attachmentDescription);
        attachmentDescriptions.push_back(attachmentDescription);
        // count attachment
        attachmentCount++;
        if (!utils::IsDepthStencil(createInfo.format))
            colorAttachmentCount++;
    }
} // namespace vks
