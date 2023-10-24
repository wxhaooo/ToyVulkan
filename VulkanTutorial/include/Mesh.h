#pragma once

#include <thread>
#include <Eigen/Dense>
#include <igl/readOBJ.h>

#include "VkUtils.h"
using namespace Eigen;

// geometry
namespace Geometry
{
	// need a generic template that can load any number of vertex attributes
	struct Vertex
	{
		Vector3f position;
		Vector3f color;
		Vector2f uv;

		static VkVertexInputBindingDescription GetBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		// static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
		// {
		// 	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
		//
		// 	attributeDescriptions[0].binding = 0;
		// 	attributeDescriptions[0].location = 0;
		// 	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		// 	attributeDescriptions[0].offset = offsetof(Vertex, position);
		//
		// 	attributeDescriptions[1].binding = 0;
		// 	attributeDescriptions[1].location = 1;
		// 	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		// 	attributeDescriptions[1].offset = offsetof(Vertex, color);
		
		// 	return attributeDescriptions;
		// }

		static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescriptions()
		{
			std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
		
			attributeDescriptions[0].binding = 0;
			attributeDescriptions[0].location = 0;
			attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[0].offset = offsetof(Vertex, position);
		
			attributeDescriptions[1].binding = 0;
			attributeDescriptions[1].location = 1;
			attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributeDescriptions[1].offset = offsetof(Vertex, color);
		
			attributeDescriptions[2].binding = 0;
			attributeDescriptions[2].location = 2;
			attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
			attributeDescriptions[2].offset = offsetof(Vertex, uv);
		
			return attributeDescriptions;
		}
	};

	class Mesh
	{

	public:
		Mesh() = delete;
		Mesh(VkPhysicalDevice physicalDeviceIn, VkDevice logicalDeviceIn,
			VkCommandPool commandPoolIn, VkQueue graphicsQueueIn)
			:physicalDevice(physicalDeviceIn), logicalDevice(logicalDeviceIn),
			commandPool(commandPoolIn), graphicsQueue(graphicsQueueIn) {}
		~Mesh()
		{
			CleanUp();
		}

		uint32_t VertexNumber() { return vertexNumber; }
		uint32_t FaceNumber() { return faceNumber; }
		uint32_t IndexNumber() { return FaceNumber() * 3; }

		VkBuffer GetVertexBuffer() { return vertexBuffer; }
		VkBuffer GetIndexBuffer() { return indexBuffer; }

	private:

		void CleanUp()
		{
			vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
			vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);

			vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
			vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);
		}

		void CreateVertexBuffer()
		{
			VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			VkUtils::CreateBuffer(physicalDevice, logicalDevice, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
				stagingBuffer, stagingBufferMemory);

			void* data;
			CheckVulkanResult(vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data));
			memcpy(data, vertices.data(), (size_t)bufferSize);
			vkUnmapMemory(logicalDevice, stagingBufferMemory);

			VkUtils::CreateBuffer(physicalDevice, logicalDevice, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				vertexBuffer, vertexBufferMemory);

			VkUtils::CopyBuffer(logicalDevice, commandPool, graphicsQueue, 
				stagingBuffer, vertexBuffer, bufferSize);

			vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
			vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
		}

		void CreateIndexBuffer()
		{
			VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

			VkBuffer stagingBuffer;
			VkDeviceMemory stagingBufferMemory;
			VkUtils::CreateBuffer(physicalDevice, logicalDevice, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				stagingBuffer, stagingBufferMemory);

			void* data;
			CheckVulkanResult(vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data));
			memcpy(data, indices.data(), (size_t)bufferSize);
			vkUnmapMemory(logicalDevice, stagingBufferMemory);

			VkUtils::CreateBuffer(physicalDevice, logicalDevice, bufferSize,
				VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				indexBuffer, indexBufferMemory);

			VkUtils::CopyBuffer(logicalDevice, commandPool, graphicsQueue,
				stagingBuffer, indexBuffer, bufferSize);

			vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
			vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
		}

	private:
		VkBuffer vertexBuffer;
		VkDeviceMemory vertexBufferMemory;
		VkBuffer indexBuffer;
		VkDeviceMemory indexBufferMemory;

		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
		VkCommandPool commandPool;
		VkQueue graphicsQueue;

#pragma region MultiThreads Loader

	public:

		void LoadFromObjFile(std::string geometryPath)
		{
			igl::readOBJ(geometryPath, rawV, rawF);

			vertexNumber = static_cast<uint32_t>(rawV.rows());
			faceNumber = static_cast<uint32_t>(rawF.rows());

			rawV.transposeInPlace();
			rawF.transposeInPlace();

			vertices.resize(vertexNumber);
			indices.resize(faceNumber * 3);

			// load vertices and faces using multi-threads
			std::thread loadVert(&Mesh::LoadFaces, this);
			std::thread loadFace(&Mesh::LoadVertices, this);

			loadVert.join();
			loadFace.join();

			CreateVertexBuffer();
			CreateIndexBuffer();
		}

	private:
		void LoadVertices()
		{
			for (uint32_t i = 0; i < vertexNumber; i++)
			{
				auto& vertex = vertices[i];
				vertex.position = rawV.col(i);
			}
		}

		void LoadFaces()
		{
			for (uint32_t i = 0; i < faceNumber; i++)
			{
				Vector3i faceIndices = static_cast<Vector3i>(rawF.col(i));
				indices[i * 3 + 0] = faceIndices.x();
				indices[i * 3 + 1] = faceIndices.y();
				indices[i * 3 + 2] = faceIndices.z();
			}
		}

	private:
		MatrixXf rawV;
		MatrixXi rawF;

		uint32_t vertexNumber;
		uint32_t faceNumber;

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

#pragma endregion MultiThreads

#pragma region DefaultModel
	public:
		void LoadSimpleTriangle()
		{
			vertexNumber = 3;
			faceNumber = 1;

			vertices.resize(vertexNumber);
			indices.resize(faceNumber * 3);

			vertices = {
				{{0.0f, -0.5f,0.0f}, {1.0f, 0.0f, 0.0f}},
				{{0.5f, 0.5f,0.0f}, {0.0f, 1.0f, 0.0f}},
				{{-0.5f, 0.5f,0.0f}, {0.0f, 0.0f, 1.0f}}
			};

			indices = { 0, 1, 2 };

			CreateVertexBuffer();
			CreateIndexBuffer();
		}

		void LoadSimpleRectangle()
		{
			vertexNumber = 4;
			faceNumber = 2;

			vertices.resize(vertexNumber);
			indices.resize(faceNumber * 3);

			// vertices = {
			// 	{{-0.5f, -0.5f,0.0f}},
			// 	{{0.5f, -0.5f,0.0f}},
			// 	{{0.5f, 0.5f,0.0f}},
			// 	{{-0.5f, 0.5f,0.0f}}
			// };

			vertices = {
				{{-0.5f, -0.5f,0.0f}, {1.0f, 0.0f, 0.0f}},
				{{0.5f, -0.5f,0.0f}, {0.0f, 1.0f, 0.0f}},
				{{0.5f, 0.5f,0.0f}, {0.0f, 0.0f, 1.0f}},
				{{-0.5f, 0.5f,0.0f}, {1.0f, 1.0f, 1.0f}}
			};

			indices = { 0, 1, 2, 2, 3, 0 };

			CreateVertexBuffer();
			CreateIndexBuffer();
		}

		void LoadTexturedRectangle()
		{
			vertexNumber = 4;
			faceNumber = 2;

			vertices.resize(vertexNumber);
			indices.resize(faceNumber * 3);

			vertices = {
	{{-0.5f, -0.5f,0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, -0.5f,0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, 0.5f,0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
	{{-0.5f, 0.5f,0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
			};

			indices = { 0, 1, 2, 2, 3, 0 };

			CreateVertexBuffer();
			CreateIndexBuffer();
		}

		void LoadOverlappingTexturedRectangle()
		{
			vertexNumber = 8;
			faceNumber = 4;

			vertices.resize(vertexNumber);
			indices.resize(faceNumber * 3);

			vertices = {
	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

	{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
	{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
	{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
	{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
			};

			indices = { 0, 1, 2, 2, 3, 0,
				4, 5, 6, 6, 7, 4 };

			CreateVertexBuffer();
			CreateIndexBuffer();
		}
#pragma endregion DefaultModel
	};
}