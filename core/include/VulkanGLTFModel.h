#pragma once

#include <VulkanDevice.h>
#include <iostream>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <VulkanTexture.h>

#include <GraphicSettings.hpp>

namespace tinygltf
{
	class Model;
	class Node;
}

namespace vks
{
	namespace geometry
	{

		extern VkDescriptorSetLayout descriptorSetLayoutImage;
		extern VkDescriptorSetLayout descriptorSetLayoutUbo;
		extern VkMemoryPropertyFlags memoryPropertyFlags;

		void LoadTextureFromGLTFImage(Texture* texture, tinygltf::Image& gltfImage, std::string path, vks::VulkanDevice* device, VkQueue copyQueue);
		
		enum DescriptorBindingFlags {
			ImageBaseColor = 0x00000001,
			ImageNormalMap = 0x00000002
		};

		enum FileLoadingFlags {
			None = 0x00000000,
			PreTransformVertices = 0x00000001,
			PreMultiplyVertexColors = 0x00000002,
			FlipY = 0x00000004,
			DontLoadImages = 0x00000008
		};

		enum RenderFlags {
			BindImages = 0x00000001,
			RenderOpaqueNodes = 0x00000002,
			RenderAlphaMaskedNodes = 0x00000004,
			RenderAlphaBlendedNodes = 0x00000008
		};

		struct Light
		{
			alignas(4)float intensity;
			alignas(16)glm::vec4 position;
			alignas(16)glm::vec4 color;
		};
		
		// Contains everything required to render a glTF model in Vulkan
		// This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
		class VulkanGLTFModel
		{
		public:
			// The class requires some Vulkan objects so it can create it's own resources
			vks::VulkanDevice* vulkanDevice;
			// copyQueue == transferQueue
			VkQueue copyQueue;

			// descriptor info
			VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

			/*
				glTF default vertex layout with easy Vulkan mapping functions
			*/
			enum class VertexComponent { Position, Normal, UV, Color, Tangent, Joint0, Weight0 };

			struct Vertex {
				glm::vec3 pos;
				glm::vec3 normal;
				glm::vec2 uv;
				glm::vec4 color;
				glm::vec4 joint0;
				glm::vec4 weight0;
				glm::vec4 tangent;
				static VkVertexInputBindingDescription vertexInputBindingDescription;
				static std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions;
				static VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
				static VkVertexInputBindingDescription InputBindingDescription(uint32_t binding);
				static VkVertexInputAttributeDescription InputAttributeDescription(uint32_t binding, uint32_t location, VertexComponent component);
				static std::vector<VkVertexInputAttributeDescription> InputAttributeDescriptions(uint32_t binding, const std::vector<VertexComponent> components);
				/** @brief Returns the default pipeline vertex input state create info structure for the requested vertex components */
				static VkPipelineVertexInputStateCreateInfo* GetPipelineVertexInputState(const std::vector<VertexComponent> components);
			};

			// Single vertex buffer for all primitives
			struct {
				VkBuffer buffer;
				VkDeviceMemory memory;
			} vertices;

			// Single index buffer for all primitives
			struct {
				int count;
				VkBuffer buffer;
				VkDeviceMemory memory;
			} indices;

			// The following structures roughly represent the glTF scene structure
			// To keep things simple, they only contain those properties that are required for this sample
			struct Node;

			struct HasSampler
			{
				alignas(4) bool hasBaseColor;
				alignas(4) bool hasNormal;
				alignas(4) bool hasRoughness;
				alignas(4) bool hasEmissive;
				alignas(4) bool hasOcclusion;
			};

			struct HasSamplerUBO
			{
				vks::Buffer buffer;
				struct Values
				{
					HasSampler* hasSampler;
				} values;
			} samplerUbo;

			// A glTF material stores information in e.g. the texture that is attached to it and colors
			struct Material {
				int index = 0;
				VulkanDevice* device = nullptr;
				enum AlphaMode { ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
				AlphaMode alphaMode = ALPHAMODE_OPAQUE;
				float alphaCutoff = 1.0f;
				float metallicFactor = 1.0f;
				float roughnessFactor = 1.0f;
				glm::vec4 baseColorFactor = glm::vec4(1.0f);
				Texture* baseColorTexture = nullptr;
				Texture* metallicRoughnessTexture = nullptr;
				Texture* normalTexture = nullptr;
				Texture* occlusionTexture = nullptr;
				Texture* emissiveTexture = nullptr;

				Texture* specularGlossinessTexture;
				Texture* diffuseTexture;

				VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

				Material(vks::VulkanDevice* device) : device(device) {}

				void Destory();
				void CreateDescriptorSet(VkDescriptorPool descriptorPool, VkDescriptorSetLayout descriptorSetLayout, uint32_t descriptorBindingFlags);
			};

			/*
				glTF primitive
			*/
			struct Primitive {
				uint32_t firstIndex;
				uint32_t indexCount;
				uint32_t firstVertex;
				uint32_t vertexCount;
				Material& material;

				struct Dimensions {
					glm::vec3 min = glm::vec3(FLT_MAX);
					glm::vec3 max = glm::vec3(-FLT_MAX);
					glm::vec3 size;
					glm::vec3 center;
					float radius;
				} dimensions;

				void SetDimensions(glm::vec3 min, glm::vec3 max);
				Primitive(uint32_t firstIndex, uint32_t indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), material(material) {}
			};

			/*
				glTF mesh
			*/
			struct Mesh {
				vks::VulkanDevice* device;

				std::vector<Primitive*> primitives;
				std::string name;

				struct UniformBuffer {
					VkBuffer buffer;
					VkDeviceMemory memory;
					VkDescriptorBufferInfo descriptor;
					VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
					void* mapped;
				} uniformBuffer;

				struct UniformBlock {
					glm::mat4 matrix;
					glm::mat4 jointMatrix[64]{};
					float jointcount{ 0 };
				} uniformBlock;

				Mesh(vks::VulkanDevice* device, glm::mat4 matrix);
				~Mesh();
			};

			/*
				glTF skin
			*/
			struct Skin {
				std::string name;
				Node* skeletonRoot = nullptr;
				std::vector<glm::mat4> inverseBindMatrices;
				std::vector<Node*> joints;
			};

			/*
				glTF node
			*/
			struct Node {
				Node* parent;
				uint32_t index;
				std::vector<Node*> children;
				glm::mat4 matrix;
				std::string name;
				Mesh* mesh;
				Skin* skin;
				int32_t skinIndex = -1;
				glm::vec3 translation{};
				glm::vec3 scale{ 1.0f };
				glm::quat rotation{};
				glm::mat4 LocalMatrix();
				glm::mat4 GetMatrix();
				void Update();
				~Node();
			};

			/*
				Model data
			*/
			std::vector<Texture> textures;
			std::vector<Material> materials;
			std::vector<Light> lights;
			std::vector<Node*> nodes;
			std::map<std::string, Node*> nodeName2LinearNodeMap;
			std::vector<Node*> linearNodes;
			std::vector<Skin*> skins;

			struct Dimensions {
				glm::vec3 min = glm::vec3(FLT_MAX);
				glm::vec3 max = glm::vec3(-FLT_MAX);
				glm::vec3 size;
				glm::vec3 center;
				float radius;
			} dimensions;

			Texture emptyTexture;

			bool metallicRoughnessWorkflow = true;
			bool buffersBound = false;
			std::string path;

			~VulkanGLTFModel();

			/*
				glTF loading functions
				The following functions take a glTF input model loaded via tinyglTF and convert all required data into our own structure
			*/
			void LoadImages(tinygltf::Model& input);
			void LoadMaterials(tinygltf::Model& input);
			void LoadLights(tinygltf::Model& input, uint32_t fileLoadingFlags);
			void LoadNode(Node *parent, const tinygltf::Node &node, uint32_t nodeIndex, const tinygltf::Model &model, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer, float globalScale);

			void LoadGLTFFile(std::string fileName,VulkanDevice* vulkanDevice, VkQueue transferQueue, uint32_t fileLoadingFlags = FileLoadingFlags::None, uint32_t descriptorBindingFlags = DescriptorBindingFlags::ImageBaseColor, float scale = 1.0f);
			/*
				glTF rendering functions
			*/
			void BindBuffers(VkCommandBuffer commandBuffer);
			// Draw a single node including child nodes (if present)
			void DrawNode(Node* node, VkCommandBuffer commandBuffer, bool pushConstant, uint32_t renderFlags = 0, VkPipelineLayout pipelineLayout = VK_NULL_HANDLE, uint32_t bindImageSet = 1);
			// Draw the glTF scene starting at the top-level-nodes
			void Draw(VkCommandBuffer commandBuffer, uint32_t renderFlags, bool pushConstant,  VkPipelineLayout pipelineLayout, uint32_t bindImageSet);

		private:
			void CreateEmptyTexture(VkQueue transferQueue);
			Texture* GetTexture(uint32_t index);
			void GetSceneDimensions();
			void GetNodeDimensions(Node *node, glm::vec3 &min, glm::vec3 &max);
			void PrepareNodeDescriptor(Node* node, VkDescriptorSetLayout descriptorSetLayout);
			// void LoadAnimations(tinygltf::Model &gltfModel);
			// void LoadSkins(tinygltf::Model& gltfModel);
		};
	}
}
