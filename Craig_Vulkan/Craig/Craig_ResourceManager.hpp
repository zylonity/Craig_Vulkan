#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <chrono>

#include "Craig_Constants.hpp"

namespace Craig {

	class ResourceManager {

	public:
		CraigError init();
		CraigError terminate();

		//Vertex buffer stuff
		struct Vertex {
			glm::vec3 m_pos;
			glm::vec3 m_color;
			glm::vec2 m_texCoord;

			static vk::VertexInputBindingDescription getBindingDescription(); //A vertex binding describes at which rate to load data from memory throughout the vertices. It specifies the number of bytes between data entries and whether to move to the next data entry after each vertex or after each instance.
			static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions(); //We have two attributes, position and color, so we need two attribute description structs.

		};

		struct SubMesh {
			uint32_t firstIndex;
			uint32_t indexCount;
			uint32_t firstVertex;
			int      materialIndex; // prim.material

			uint32_t m_vertexCount = 0;
			Vertex* m_verticies;

			uint32_t m_indexCount = 0;
			uint16_t* m_indicies;

		};

		struct Model {
			std::vector<SubMesh*> subMeshes;
			uint32_t subMeshesCount;

		};


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
	};



}