#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Craig_Constants.hpp"
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>
#include "../External/vk_mem_alloc.h"


namespace Craig {

	class Renderer;

	//Vertex buffer
	struct Vertex {
		glm::vec3 m_pos;
		glm::vec3 m_color;
		glm::vec2 m_texCoord;

		static vk::VertexInputBindingDescription getBindingDescription(); //A vertex binding describes at which rate to load data from memory throughout the vertices. It specifies the number of bytes between data entries and whether to move to the next data entry after each vertex or after each instance.
		static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions(); //We have two attributes, position and color, so we need two attribute description structs.

	};

	struct SubMesh
	{
		std::vector<Vertex> m_vertices;
		std::vector<uint32_t> m_indices;

		uint32_t vertexOffset = 0;
		uint32_t indexOffset = 0;

		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t firstVertex;
		int      materialIndex; // prim.material
	};

	struct Texture
	{
		uint32_t      m_VK_mipLevels = 0;

		vk::Image     m_VK_textureImage;
		VmaAllocation m_VMA_textureImageAllocation;

		vk::ImageView m_VK_textureImageView;

	};

	struct Model {
		std::vector<Craig::SubMesh*> subMeshes;
		uint32_t subMeshesCount;
		std::string modelPath;
		Craig::Texture m_texture;

	};

	

	class ResourceManager {

	public:
		CraigError init(Craig::Renderer* rendererToSet);
		CraigError terminate();

		void loadModel(std::string modelPath);
		void terminateModel();

		Craig::Model& getModel(std::string modelPath) { return m_testModel; };

		//===============================================================================
		// Singleton Implementations
		static ResourceManager& getInstance()
		{
			static ResourceManager instance; // Guaranteed to be destroyed.
			return instance;
		}
		// Make deleted functions public for nicer error messages (~ Scott Myers)
		ResourceManager(ResourceManager const&) = delete;	// Copy constructor
		void operator=(ResourceManager const&) = delete;	// Assignment Operator
		//===============================================================================
	private:

		//===============================================================================
		// Singleton Implementations (Banned functions to prevent a new instance)
		ResourceManager() {}										// Default Constructor private so can only be called from within
		//===============================================================================

		Craig::Renderer* m_renderer;
		Craig::Model m_testModel;
	};



}