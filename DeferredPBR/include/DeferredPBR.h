#pragma once
#include <iostream>
#include <VulkanApplicationBase.h>
#include <VulkanGLTFModel.h>
#include <VulkanRenderPass.h>

class DeferredPBR :public VulkanApplicationBase
{
public:
    DeferredPBR():VulkanApplicationBase("Deferred PBR",1920,960){}
    ~DeferredPBR() override;

    void InitFondation() override;
    void Prepare() override;
    void LoadAsset();
    void PrepareUniformBuffers();
    void Render() override;
    void BuildCommandBuffers(VkCommandBuffer commandBuffer) override;
    void NewGUIFrame() override;
    void PrepareRenderPass(VkCommandBuffer commandBuffer) override;
    void ReCreateVulkanResource_Child() override;

protected:
    void ViewChanged() override;

private:
    void UpdateUniformBuffers();
    void SetupDescriptors();
    void PreparePipelines();
    void SetupMrtRenderPass();
    void SetupLightingRenderPass();

    std::unique_ptr<vks::geometry::VulkanGLTFModel> gltfModel;
    struct ShaderData {
        vks::Buffer buffer;
        struct Values {
            glm::mat4 projection;
            glm::mat4 view;
        } values;
    } shaderData;

    std::unique_ptr<vks::VulkanRenderPass> mrtRenderPass = nullptr;
    std::unique_ptr<vks::VulkanRenderPass> lightingRenderPass = nullptr;

    VkDescriptorSetLayout MVPDescriptorSetLayout;

    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet;

    struct Pipelines {
        VkPipeline offscreen = VK_NULL_HANDLE;
        VkPipeline offscreenWireframe = VK_NULL_HANDLE;
    } pipelines;
};
