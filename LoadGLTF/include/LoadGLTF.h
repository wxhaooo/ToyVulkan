#pragma once
#include <iostream>
#include <VulkanApplicationBase.h>

#include <VulkanGLTFModel.h>

class LoadGLFT :public VulkanApplicationBase
{
public:
    LoadGLFT(bool validation):VulkanApplicationBase("Load GLTF",1280,960,validation){}
    ~LoadGLFT() override;

    void Prepare() override;
    void LoadAsset();
    void PrepareUniformBuffers();
    void Render() override;
    void SetupCamera() override;
    void BuildCommandBuffers() override;

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

    struct DescriptorSetLayouts {
        VkDescriptorSetLayout matrices;
        VkDescriptorSetLayout textures;
    } descriptorSetLayouts;

    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet;

    struct Pipelines {
        VkPipeline solid;
        VkPipeline wireframe = VK_NULL_HANDLE;
    } pipelines;
};
