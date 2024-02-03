#pragma once
#include <VulkanDevice.h>
#include <VulkanFrameBuffer.h>
#include <map>
#include <memory>

namespace vks {
struct AttachmentCreateInfo;
class VulkanFrameBuffer;
} // namespace vks

namespace vks {
// for loadGLTF.cpp
struct OffscreenPass {
  VkDevice device = VK_NULL_HANDLE;
  int32_t width, height;
  VkRenderPass renderPass;
  std::vector<VkFramebuffer> frameBuffer;
  std::vector<vks::FramebufferAttachment> color, depth;
  std::vector<VkSampler> sampler;
  std::vector<VkDescriptorImageInfo> descriptor;

  VkDescriptorSetLayout descriptorSetLayout;
  VkDescriptorPool descriptorPool;
  std::vector<VkDescriptorSet> descriptorSet;

  ~OffscreenPass();
  void DestroyResource();
};

/* general render pass and subPass */
class VulkanSubPass {
public:
  VulkanSubPass() = delete;
  VulkanSubPass(const std::string &subPassName,
                VkPipelineBindPoint subPassBindPoint, VulkanDevice *device);

  void AddAttachments(FrameBuffer *frameBuffer,
                      const std::vector<int32_t> &attachmentIndices);
  void CreateDescription();
  VkSubpassDescription& GetDescription();

private:
  std::vector<VkAttachmentReference> colorReferences;
  VkAttachmentReference depthReference = {};

  bool hasDepth = false;
  bool hasColor = false;

  VkPipelineBindPoint bindPoint;
  VkSubpassDescription description;
  VulkanDevice *vulkanDevice = nullptr;
  std::string name;
};

class VulkanRenderPass {
public:
  VulkanRenderPass() = delete;
  VulkanRenderPass(const std::string &renderPassName, VulkanDevice *device);
  ~VulkanRenderPass();

  VkRenderPass renderPass;

  // frame buffer
  std::unique_ptr<VulkanFrameBuffer> vulkanFrameBuffer = nullptr;
  FrameBuffer *templateFrameBuffer = nullptr;

  // subPass info
  std::vector<VkSubpassDependency> subPassDependencies;
  std::vector<VulkanSubPass *> subPassArray;
  std::map<std::string, std::unique_ptr<VulkanSubPass>> subPassName2InstMap;

  void Init(uint32_t width, uint32_t height, uint32_t maxFrameInFlight);

  void Destroy();

  void AddSampler(VkFilter magFilter, VkFilter minFilter,
                  VkSamplerAddressMode addressMode);

  void AddAttachment(const vks::AttachmentCreateInfo &createInfo);

  void AddAttachments(
      std::vector<vks::FramebufferAttachment> &existedFrameBufferAttachments);

  void AddSubPass(const std::string &subPassName,
                  VkPipelineBindPoint subPassBindPoint,
                  const std::vector<std::string> &attachmentNames);

  void AddSubPass(const std::string &subPassName,
                  VkPipelineBindPoint subPassBindPoint,
                  const std::vector<int32_t> &attachmentIndices);

  void AddSubPassDependency(std::vector<VkSubpassDependency> subPassDependency);

  void CreateRenderPass();

  // all attachment and single subPass
  void CreateDefaultRenderPass();

  void CreateDescriptorSet();

  int AttachmentCount();

  int ColorAttachmentCount();

private:
  std::string name;
  int attachmentCount = 0;
  int colorAttachmentCount = 0;
  VulkanDevice *vulkanDevice = nullptr;
};
} // namespace vks
