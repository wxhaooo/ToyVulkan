#pragma once
#include <iostream>
#include <VulkanApplicationBase.h>
#include <VulkanGLTFModel.h>
#include <VulkanRenderPass.h>

class LoadGLFT :public VulkanApplicationBase
{
public:
    LoadGLFT():VulkanApplicationBase("Load GLTF",1280,960){}
    ~LoadGLFT() override;

    void InitFondation() override;
    void Prepare() override;
    void LoadAsset();
    void PrepareUniformBuffers();
    void Render() override;
    void BuildCommandBuffers(VkCommandBuffer commandBuffer) override;
    void NewGUIFrame() override;
    void ReCreateVulkanResource_Child() override;
    void PrepareRenderPass(VkCommandBuffer commandBuffer) override;

protected:
    void ViewChanged() override;

private:
    void UpdateUniformBuffers();
    void SetupDescriptors();
    void PreparePipelines();

    /** @brief Setup offscreen renderpass **/
    void SetupOffscreenRenderPass();
    /** @brief Setup offscreen vulkan resource (image view and frame buffer.etc) */
    void SetupOffscreenResource();
    /** @brief Setup offscreen framebuffer */
    void SetupOffscreenFrameBuffer();

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

    std::unique_ptr<vks::OffscreenPass> offscreenPass;

    struct Pipelines {
        VkPipeline onscreen = VK_NULL_HANDLE;
        VkPipeline offscreen = VK_NULL_HANDLE;
        VkPipeline offscreenWireframe = VK_NULL_HANDLE;
        VkPipeline onScreenWireframe = VK_NULL_HANDLE;
    } pipelines;
};
