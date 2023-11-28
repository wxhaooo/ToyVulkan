﻿#include <VulkanGLTFModel.h>
#include <VulkanHelper.h>
#ifdef WIN32
#undef min
#undef max
#endif
#include <tiny_gltf.h>
#include <VulkanInitializers.h>

namespace vks
{
	namespace geometry
	{
		/*
		We use a custom image loading function with tinyglTF, so we can do custom stuff loading ktx textures
		*/
		bool loadImageDataFunc(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
		{
			// KTX files will be handled by our own code
			if (image->uri.find_last_of(".") != std::string::npos) {
				if (image->uri.substr(image->uri.find_last_of(".") + 1) == "ktx") {
					return true;
				}
			}

			return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
		}

		bool loadImageDataFuncEmpty(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData) 
		{
			// This function will be used for samples that don't require images to be loaded
			return true;
		}
		
			// Contains everything required to render a glTF model in Vulkan
			// This class is heavily simplified (compared to glTF's feature set) but retains the basic glTF structure
			VulkanGLTFModel::~VulkanGLTFModel()
			{
				vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, textureDescriptorSetLayout, nullptr);
				vkDestroyDescriptorPool(vulkanDevice->logicalDevice,descriptorPool,nullptr);
				
				for (auto node : nodes) {
					delete node;
				}
				// Release all Vulkan resources allocated for the model
				vkDestroyBuffer(vulkanDevice->logicalDevice, vertices.buffer, nullptr);
				vkFreeMemory(vulkanDevice->logicalDevice, vertices.memory, nullptr);
				vkDestroyBuffer(vulkanDevice->logicalDevice, indices.buffer, nullptr);
				vkFreeMemory(vulkanDevice->logicalDevice, indices.memory, nullptr);
				for (Image image : images) {
					vkDestroyImageView(vulkanDevice->logicalDevice, image.texture.view, nullptr);
					vkDestroyImage(vulkanDevice->logicalDevice, image.texture.image, nullptr);
					vkDestroySampler(vulkanDevice->logicalDevice, image.texture.sampler, nullptr);
					vkFreeMemory(vulkanDevice->logicalDevice, image.texture.deviceMemory, nullptr);
				}
			}

			/*
				glTF loading functions
		
				The following functions take a glTF input model loaded via tinyglTF and convert all required data into our own structure
			*/
			void VulkanGLTFModel::LoadImages(tinygltf::Model& input)
			{
				// Images can be stored inside the glTF (which is the case for the sample model), so instead of directly
				// loading them from disk, we fetch them from the glTF loader and upload the buffers
				images.resize(input.images.size());
				for (size_t i = 0; i < input.images.size(); i++) {
					tinygltf::Image& glTFImage = input.images[i];
					// Get the image data from the glTF loader
					unsigned char* buffer = nullptr;
					VkDeviceSize bufferSize = 0;
					bool deleteBuffer = false;
					// We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
					if (glTFImage.component == 3) {
						bufferSize = glTFImage.width * glTFImage.height * 4;
						buffer = new unsigned char[bufferSize];
						unsigned char* rgba = buffer;
						unsigned char* rgb = &glTFImage.image[0];
						for (size_t i = 0; i < glTFImage.width * glTFImage.height; ++i) {
							memcpy(rgba, rgb, sizeof(unsigned char) * 3);
							rgba += 4;
							rgb += 3;
						}
						deleteBuffer = true;
					}
					else {
						buffer = &glTFImage.image[0];
						bufferSize = glTFImage.image.size();
					}
					// Load texture from image buffer
					images[i].texture.FromBuffer(buffer, bufferSize, VK_FORMAT_R8G8B8A8_UNORM, glTFImage.width, glTFImage.height, vulkanDevice, copyQueue);
					if (deleteBuffer) {
						delete[] buffer;
					}
				}
			}
			void VulkanGLTFModel::LoadTextures(tinygltf::Model& input)
			{
				textures.resize(input.textures.size());
				for (size_t i = 0; i < input.textures.size(); i++) {
					textures[i].imageIndex = input.textures[i].source;
				}
			}
			void VulkanGLTFModel::LoadMaterials(tinygltf::Model& input)
			{
				materials.resize(input.materials.size());
				for (size_t i = 0; i < input.materials.size(); i++) {
					// We only read the most basic properties required for our sample
					tinygltf::Material glTFMaterial = input.materials[i];
					// Get the base color factor
					if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
						materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
					}
					// Get base color texture index
					if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
						materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
					}
				}
			}
			void VulkanGLTFModel::LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, Node* parent, std::vector<uint32_t>& indexBuffer, std::vector<Vertex>& vertexBuffer)
			{
				VulkanGLTFModel::Node* node = new VulkanGLTFModel::Node{};
				node->matrix = glm::mat4(1.0f);
				node->parent = parent;

				// Get the local node matrix
				// It's either made up from translation, rotation, scale or a 4x4 matrix
				if (inputNode.translation.size() == 3) {
					node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
				}
				if (inputNode.rotation.size() == 4) {
					glm::quat q = glm::make_quat(inputNode.rotation.data());
					node->matrix *= glm::mat4(q);
				}
				if (inputNode.scale.size() == 3) {
					node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
				}
				if (inputNode.matrix.size() == 16) {
					node->matrix = glm::make_mat4x4(inputNode.matrix.data());
				};

				// Load node's children
				if (inputNode.children.size() > 0) {
					for (size_t i = 0; i < inputNode.children.size(); i++) {
						LoadNode(input.nodes[inputNode.children[i]], input , node, indexBuffer, vertexBuffer);
					}
				}

				// If the node contains mesh data, we load vertices and indices from the buffers
				// In glTF this is done via accessors and buffer views
				if (inputNode.mesh > -1) {
					const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
					// Iterate through all primitives of this node's mesh
					for (size_t i = 0; i < mesh.primitives.size(); i++) {
						const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
						uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
						uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
						uint32_t indexCount = 0;
						// Vertices
						{
							const float* positionBuffer = nullptr;
							const float* normalsBuffer = nullptr;
							const float* texCoordsBuffer = nullptr;
							size_t vertexCount = 0;

							// Get buffer data for vertex positions
							if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
								const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
								const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
								positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
								vertexCount = accessor.count;
							}
							// Get buffer data for vertex normals
							if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
								const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
								const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
								normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
							}
							// Get buffer data for vertex texture coordinates
							// glTF supports multiple sets, we only load the first one
							if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
								const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
								const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
								texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
							}

							// Append data to model's vertex buffer
							for (size_t v = 0; v < vertexCount; v++) {
								Vertex vert{};
								vert.pos = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
								vert.normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
								vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
								vert.color = glm::vec3(1.0f);
								vertexBuffer.push_back(vert);
							}
						}
						// Indices
						{
							const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
							const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
							const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

							indexCount += static_cast<uint32_t>(accessor.count);

							// glTF supports different component types of indices
							switch (accessor.componentType) {
							case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
									const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
									for (size_t index = 0; index < accessor.count; index++) {
										indexBuffer.push_back(buf[index] + vertexStart);
									}
									break;
							}
							case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
									const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
									for (size_t index = 0; index < accessor.count; index++) {
										indexBuffer.push_back(buf[index] + vertexStart);
									}
									break;
							}
							case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
									const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
									for (size_t index = 0; index < accessor.count; index++) {
										indexBuffer.push_back(buf[index] + vertexStart);
									}
									break;
							}
							default:
								std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
								return;
							}
						}
						Primitive primitive{};
						primitive.firstIndex = firstIndex;
						primitive.indexCount = indexCount;
						primitive.materialIndex = glTFPrimitive.material;
						node->mesh.primitives.push_back(primitive);
					}
				}

				if (parent) {
					parent->children.push_back(node);
				}
				else {
					nodes.push_back(node);
				}
			}

			void VulkanGLTFModel::LoadGLTFFile(std::string fileName,VulkanDevice* vulkanDevice, VkQueue transferQueue, uint32_t fileLoadingFlags, float scale)
	        {
        		
	            tinygltf::Model glTFInput;
				tinygltf::TinyGLTF gltfContext;

				if (fileLoadingFlags & FileLoadingFlags::DontLoadImages) {
					gltfContext.SetImageLoader(loadImageDataFuncEmpty, nullptr);
				} else {
					gltfContext.SetImageLoader(loadImageDataFunc, nullptr);
				}
				
				std::string error, warning;

	#if defined(__ANDROID__)
			// On Android all assets are packed with the apk in a compressed form, so we need to open them using the asset manager
			// We let tinygltf handle this, by passing the asset manager of our app
				tinygltf::asset_manager = androidApp->activity->assetManager;
	#endif
				bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, fileName);

				// Pass some Vulkan resources required for setup and rendering to the glTF model loading class
				this->vulkanDevice = vulkanDevice;
				this->copyQueue = transferQueue;

				std::vector<uint32_t> indexBuffer;
				std::vector<geometry::VulkanGLTFModel::Vertex> vertexBuffer;

				if (fileLoaded) {
					LoadImages(glTFInput);
					LoadMaterials(glTFInput);
					LoadTextures(glTFInput);
					const tinygltf::Scene& scene = glTFInput.scenes[0];
					for (size_t i = 0; i < scene.nodes.size(); i++) {
						const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
						LoadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
					}
				}
				else {
					helper::ExitFatal("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.", -1);
				}

				// Create and upload vertex and index buffer
				// We will be using one single vertex buffer and one single index buffer for the whole glTF scene
				// Primitives (of the glTF model) will then index into these using index offsets

				size_t vertexBufferSize = vertexBuffer.size() * sizeof(geometry::VulkanGLTFModel::Vertex);
				size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
				indices.count = static_cast<uint32_t>(indexBuffer.size());

				struct StagingBuffer {
					VkBuffer buffer;
					VkDeviceMemory memory;
				} vertexStaging, indexStaging;

				// Create host visible staging buffers (source)
				CheckVulkanResult(vulkanDevice->CreateBuffer(
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					vertexBufferSize,
					&vertexStaging.buffer,
					&vertexStaging.memory,
					vertexBuffer.data()));
				// Index data
				CheckVulkanResult(vulkanDevice->CreateBuffer(
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					indexBufferSize,
					&indexStaging.buffer,
					&indexStaging.memory,
					indexBuffer.data()));

				// Create device local buffers (target)
				CheckVulkanResult(vulkanDevice->CreateBuffer(
					VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					vertexBufferSize,
					&vertices.buffer,
					&vertices.memory));
				CheckVulkanResult(vulkanDevice->CreateBuffer(
					VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					indexBufferSize,
					&indices.buffer,
					&indices.memory));

				// Copy data from staging buffers (host) do device local buffer (gpu)
				VkCommandBuffer copyCmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
				VkBufferCopy copyRegion = {};

				copyRegion.size = vertexBufferSize;
				vkCmdCopyBuffer(
					copyCmd,
					vertexStaging.buffer,
					vertices.buffer,
					1,
					&copyRegion);

				copyRegion.size = indexBufferSize;
				vkCmdCopyBuffer(
					copyCmd,
					indexStaging.buffer,
					indices.buffer,
					1,
					&copyRegion);

				vulkanDevice->FlushCommandBuffer(copyCmd, transferQueue, true);

				// Free staging resources
				vkDestroyBuffer(vulkanDevice->logicalDevice, vertexStaging.buffer, nullptr);
				vkFreeMemory(vulkanDevice->logicalDevice, vertexStaging.memory, nullptr);
				vkDestroyBuffer(vulkanDevice->logicalDevice, indexStaging.buffer, nullptr);
				vkFreeMemory(vulkanDevice->logicalDevice, indexStaging.memory, nullptr);
	        }

			void VulkanGLTFModel::SetupDescriptorSet()
			{
				std::vector<VkDescriptorPoolSize> poolSizes = {
				// One combined image sampler per model image/texture
				vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(images.size())),
				};
				// One set for matrices and one per model image/texture
				const uint32_t maxSetCount = static_cast<uint32_t>(images.size()) + 1;
				VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(poolSizes, maxSetCount);
				CheckVulkanResult(vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

				// Descriptor set layout for passing material textures
				VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
				VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(&setLayoutBinding, 1);

				CheckVulkanResult(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorSetLayoutCI, nullptr, &textureDescriptorSetLayout));

				// Descriptor sets for materials
				for (auto& image : images) {
					const VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool, &textureDescriptorSetLayout, 1);
					CheckVulkanResult(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &image.descriptorSet));
					VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(image.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &image.texture.descriptor);
					vkUpdateDescriptorSets(vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
				}
			}

			/*
				glTF rendering functions
			*/
			// Draw a single node including child nodes (if present)
			void VulkanGLTFModel::DrawNode(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, Node* node)
			{
				if (node->mesh.primitives.size() > 0) {
					// Pass the node's matrix via push constants
					// Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
					glm::mat4 nodeMatrix = node->matrix;
					VulkanGLTFModel::Node* currentParent = node->parent;
					while (currentParent) {
						nodeMatrix = currentParent->matrix * nodeMatrix;
						currentParent = currentParent->parent;
					}
					// Pass the final matrix to the vertex shader using push constants
					vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &nodeMatrix);
					for (VulkanGLTFModel::Primitive& primitive : node->mesh.primitives) {
						if (primitive.indexCount > 0) {
							// Get the texture index for this primitive
							VulkanGLTFModel::Texture texture = textures[materials[primitive.materialIndex].baseColorTextureIndex];
							// Bind the descriptor for the current primitive's texture
							vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &images[texture.imageIndex].descriptorSet, 0, nullptr);
							vkCmdDrawIndexed(commandBuffer, primitive.indexCount, 1, primitive.firstIndex, 0, 0);
						}
					}
				}
				for (auto& child : node->children) {
					DrawNode(commandBuffer, pipelineLayout, child);
				}
			}
			// Draw the glTF scene starting at the top-level-nodes
			void VulkanGLTFModel::Draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout)
			{
				// All vertices and indices are stored in single buffers, so we only need to bind once
				VkDeviceSize offsets[1] = { 0 };
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertices.buffer, offsets);
				vkCmdBindIndexBuffer(commandBuffer, indices.buffer, 0, VK_INDEX_TYPE_UINT32);
				// Render all nodes at top-level
				for (auto& node : nodes) {
					DrawNode(commandBuffer, pipelineLayout, node);
				}
			}
	};
}
