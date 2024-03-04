#pragma once
#include <iostream>
#include <VulkanApplicationBase.h>
#include <VulkanGLTFModel.h>
#include <VulkanRenderPass.h>

#include <GloalVars.h>

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
    // void BuildCommandBuffers(VkCommandBuffer commandBuffer) override;
    void NewGUIFrame() override;
    void PrepareRenderPass(VkCommandBuffer commandBuffer) override;
    void ReCreateVulkanResource_Child() override;

protected:
    void ViewChanged() override;

private:

    void UpdateUniformBuffers();
    void SetupDescriptorSets();

    void BakingIrradianceCubeMap();
    void BakingPreFilteringCubeMap();
    void BakingSpecularBRDFCubeMap();

    // SSAO
    void PrepareSSAOGenData();

    void PrepareMrtPipeline();
    void PrepareSSAOPipeline();
    void PrepareSSAOBlurPipeline();
    void PrepareLightingPipeline();
    void PrepareSkyboxPipeline();
    void PreparePostprocessPipeline();

    void SetupMrtRenderPass();
    void SetupSSAORenderPass();
    void SetupLightingRenderPass();
    void SetupSkyboxRenderPass();
    void SetupPostprocessRenderPass();

    std::unique_ptr<vks::geometry::VulkanGLTFModel> gltfModel;
    std::unique_ptr<vks::geometry::VulkanGLTFModel> skybox;

    struct ShaderData {
        vks::Buffer buffer;
        struct Values {
            float nearPlane;
            float farPlane;
            glm::mat4 projection;
            glm::mat4 view;
        } values;
    } mrtUBO;

    struct SsaoCreateUBO
    {
        vks::Buffer buffer;
        struct Values{
            glm::mat4 view;
            glm::mat3 invViewT;
            glm::mat4 projection;
            std::array<glm::vec4, GlobalVars::SSAO_KERNEL_SIZE> kernel;
            float ssaoRadius;
            float ssaoBias;
        }values;
    } ssaoCreateUbo;

    struct SsaoBlurUBO
    {
        vks::Buffer buffer;
        struct Values
        {
            glm::mat4 projection;
            int32_t ssaoBlur = true;
        }values;
    } ssaoUbo;

    struct LightingUBO
    {
        vks::Buffer buffer;
        struct Values
        {
            alignas(16) vks::geometry::Light lights[GlobalVars::LIGHT_COUNT];
            alignas(16) glm::vec4 viewPos;
            alignas(16) glm::mat4 viewMat;
        } values;
    } lightingUbo;

    struct SkyboxUBO
    {
        vks::Buffer buffer;
        struct Values {
            alignas(16) glm::mat4 model;
            alignas(16) glm::mat4 view;
            alignas(16) glm::mat4 projection;
        } values;
    } skyboxUbo;

    std::unique_ptr<vks::VulkanRenderPass> mrtRenderPass = nullptr;
    std::unique_ptr<vks::VulkanRenderPass> ssaoRenderPass = nullptr;
    std::unique_ptr<vks::VulkanRenderPass> lightingRenderPass = nullptr;
    std::unique_ptr<vks::VulkanRenderPass> skyboxRenderPass = nullptr;
    std::unique_ptr<vks::VulkanRenderPass> postprocessRenderPass = nullptr;

    std::unique_ptr<vks::VulkanFrameBuffer> mrtFrameBuffer = nullptr;
    std::unique_ptr<vks::VulkanFrameBuffer> ssaoFrameBuffer = nullptr;
    std::unique_ptr<vks::VulkanFrameBuffer> lightingFrameBuffer = nullptr;
    std::unique_ptr<vks::VulkanFrameBuffer> skyboxFrameBuffer = nullptr;
    std::unique_ptr<vks::VulkanFrameBuffer> postprocessFrameBuffer = nullptr;

    VkDescriptorSetLayout mrtDescriptorSetLayout_Vertex = VK_NULL_HANDLE;
    VkDescriptorSetLayout mrtDescriptorSetLayout_Fragment = VK_NULL_HANDLE;
    VkPipelineLayout mrtPipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> mrtDescriptorSets_Vertex;

    VkDescriptorSetLayout ssaoDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout ssaoPipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> ssaoDescriptorSets;

    VkDescriptorSetLayout ssaoBlurDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout ssaoBlurPipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> ssaoBlurDescriptorSets;

    VkDescriptorSetLayout lightingDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout lightingPipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> lightingDescriptorSets;

    VkDescriptorSetLayout skyboxDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout skyboxPipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> skyboxDescriptorSets;

    VkDescriptorSetLayout postprocessDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout postprocessPipelineLayout = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> postprocessDescriptorSets;

    // irradiance cube map
    std::unique_ptr<vks::TextureCubeMap> irradianceCubeMap = nullptr;
    // baking specular cube map
    std::unique_ptr<vks::TextureCubeMap> preFilteringCubeMap = nullptr;
    std::unique_ptr<vks::Texture2D> specularBRDFLut = nullptr;
    // environment cube map
    std::unique_ptr<vks::TextureCubeMap> environmentCubeMap = nullptr;
    // ssao texture
    std::unique_ptr<vks::Texture2D> ssaoNoiseTexture = nullptr;

    struct Pipelines {
        VkPipeline offscreen = VK_NULL_HANDLE;
        VkPipeline offscreenWireframe = VK_NULL_HANDLE;
        VkPipeline ssao = VK_NULL_HANDLE;
        VkPipeline ssaoBlur = VK_NULL_HANDLE;
        VkPipeline lighting = VK_NULL_HANDLE;
        VkPipeline skybox = VK_NULL_HANDLE;
        VkPipeline postprocess = VK_NULL_HANDLE;
    } pipelines;
};
