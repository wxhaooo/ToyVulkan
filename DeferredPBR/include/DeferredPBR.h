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
    void SetupDescriptorSets();
    void PrepareMrtPipeline();
    void PrepareLightingPipeline();
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

    struct LightingUBO
    {
        vks::Buffer buffer;
        struct Values
        {
            alignas(16)vks::geometry::Light lights[LightCount];
            alignas(16) glm::vec4 viewPos;
        } values;
    } lightingUbo;

    std::unique_ptr<vks::VulkanRenderPass> mrtRenderPass = nullptr;
    std::unique_ptr<vks::VulkanRenderPass> lightingRenderPass = nullptr;

    VkDescriptorSetLayout MVPDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout;
    std::vector<VkDescriptorSet> descriptorSets;

    VkDescriptorSetLayout lightingDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout lightingPipelineLayout;
    std::vector<VkDescriptorSet> lightingDescriptorSets;

    struct Pipelines {
        VkPipeline offscreen = VK_NULL_HANDLE;
        VkPipeline offscreenWireframe = VK_NULL_HANDLE;
        VkPipeline lighting = VK_NULL_HANDLE;
    } pipelines;
};
