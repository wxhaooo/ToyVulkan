#pragma once
#include <iostream>
#include <VulkanApplicationBase.h>
#include <VulkanGLTFModel.h>

class DeferredPBR :public VulkanApplicationBase
{
public:
    DeferredPBR():VulkanApplicationBase("Deferred PBR",1280,960){}
    ~DeferredPBR() override;

    void InitFondation() override;
    void Prepare() override;
    void LoadAsset();
    void PrepareUniformBuffers();
    void Render() override;
    void BuildCommandBuffers(VkCommandBuffer commandBuffer) override;
    void NewGUIFrame() override;

protected:
    void ViewChanged() override;

private:
    void UpdateUniformBuffers();
    void SetupDescriptors();
    void PreparePipelines();

    std::unique_ptr<vks::geometry::VulkanGLTFModel> gltfModel;
    struct ShaderData {
        vks::Buffer buffer;
        struct Values {
            glm::mat4 projection;
            glm::mat4 model;
            glm::vec4 lightPos = glm::vec4(5.0f, 5.0f, -5.0f, 1.0f);
            glm::vec4 viewPos;
        } values;
    } shaderData;

    VkDescriptorSetLayout MVPDescriptorSetLayout;

    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet;

    struct Pipelines {
        VkPipeline onscreen = VK_NULL_HANDLE;
        VkPipeline offscreen = VK_NULL_HANDLE;
        VkPipeline offscreenWireframe = VK_NULL_HANDLE;
        VkPipeline onScreenWireframe = VK_NULL_HANDLE;
    } pipelines;
};
