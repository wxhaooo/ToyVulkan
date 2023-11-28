#pragma once

#include <VulkanDevice.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <VulkanTexture.h>

namespace tinygltf
{
	class Model;
	class Node;
}

namespace vks
{
	namespace geometry
	{
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
		
		// Contains everything required to render a glTF model in Vulkan
		// This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
		class VulkanGLTFModel
		{
		public:
			// The class requires some Vulkan objects so it can create it's own resources
			vks::VulkanDevice* vulkanDevice;
			VkQueue copyQueue;

			// descriptor info
			VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
			VkDescriptorSetLayout textureDescriptorSetLayout;

			// The vertex layout for the samples' model
			struct Vertex {
				glm::vec3 pos;
				glm::vec3 normal;
				glm::vec2 uv;
				glm::vec3 color;
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

			// A primitive contains the data for a single draw call
			struct Primitive {
				uint32_t firstIndex;
				uint32_t indexCount;
				int32_t materialIndex;
			};

			// Contains the node's (optional) geometry and can be made up of an arbitrary number of primitives
			struct Mesh {
				std::vector<Primitive> primitives;
			};

			// A node represents an object in the glTF scene graph
			struct Node {
				Node* parent;
				std::vector<Node*> children;
				Mesh mesh;
				glm::mat4 matrix;
				~Node() {
					for (auto& child : children) {
						delete child;
					}
				}
			};

			// A glTF material stores information in e.g. the texture that is attached to it and colors
			struct Material {
				glm::vec4 baseColorFactor = glm::vec4(1.0f);
				uint32_t baseColorTextureIndex;
			};

			// Contains the texture for a single glTF image
			// Images may be reused by texture objects and are as such separated
			struct Image {
				Texture2D texture;
				// We also store (and create) a descriptor set that's used to access this texture from the fragment shader
				VkDescriptorSet descriptorSet;
			};

			// A glTF texture stores a reference to the image and a sampler
			// In this sample, we are only interested in the image
			struct Texture {
				int32_t imageIndex;
			};

			/*
				Model data
			*/
			std::vector<Image> images;
			std::vector<Texture> textures;
			std::vector<Material> materials;
			std::vector<Node*> nodes;

			~VulkanGLTFModel();

			/*
				glTF loading functions
				The following functions take a glTF input model loaded via tinyglTF and convert all required data into our own structure
			*/
			void LoadImages(tinygltf::Model& input);
			void LoadTextures(tinygltf::Model& input);
			void LoadMaterials(tinygltf::Model& input);
			void LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer);

			void LoadGLTFFile(std::string fileName,VulkanDevice* vulkanDevice, VkQueue transferQueue, uint32_t fileLoadingFlags = FileLoadingFlags::None, float scale = 1.0f);
			void SetupDescriptorSet();
			/*
				glTF rendering functions
			*/
			// Draw a single node including child nodes (if present)
			void DrawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node);
			// Draw the glTF scene starting at the top-level-nodes
			void Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout);
		};
	}
}
