#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Craig_Constants.hpp"
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>
#include <unordered_map>


namespace Craig {

	class Renderer;

	//Vertex buffer
	struct Vertex {
		glm::vec3 m_pos;
		glm::vec3 m_color;
		glm::vec3 m_normals;
		glm::vec2 m_texCoord;


		static vk::VertexInputBindingDescription getBindingDescription(); //A vertex binding describes at which rate to load data from memory throughout the vertices. It specifies the number of bytes between data entries and whether to move to the next data entry after each vertex or after each instance.
		static std::array<vk::VertexInputAttributeDescription, kVertexAttributeDescriptors> getAttributeDescriptions(); //We have two attributes, position and color, so we need two attribute description structs.

	};

	// Sent per draw, has to match PushConstants in both shaders
	// Order matters, mat4 + vec4 first keeps the offsets the same under any layout rules
	struct PushConstantData {
		glm::mat4 nodeMatrix;       // The node's transform inside the model
		glm::vec4 baseColorFactor;  // Material colour, multiplied into the texture
		uint32_t  objectIndex;      // Which slot of the transforms SSBO to read
	};

	// The glTF structure below is adapted from Sascha Willems' gltfloading example (MIT)
	// https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfloading/gltfloading.cpp

	// One glTF primitive = one draw call, with its own material
	struct SubMesh
	{
		std::vector<Vertex> m_vertices;
		std::vector<uint32_t> m_indices;

		uint32_t vertexOffset = 0;
		uint32_t indexOffset = 0;

		uint32_t indexCount = 0;
		int32_t  materialIndex = -1; // prim.material, -1 = no material
	};

	// A GPU texture, one per glTF image (Sascha calls this an "Image")
	struct Texture
	{
		uint32_t      m_VK_mipLevels = 0;

		vk::Image     m_VK_textureImage;
		VmaAllocation m_VMA_textureImageAllocation;

		vk::ImageView m_VK_textureImageView;

		vk::DescriptorSet m_VK_descriptorSet; // Made by the renderer, null until then
	};

	// A glTF texture just points at an image, images can be shared between textures
	struct GltfTexture
	{
		int32_t imageIndex = -1;
	};

	struct Material
	{
		glm::vec4 baseColorFactor = glm::vec4(1.0f);
		int32_t baseColorTextureIndex = -1;
	};

	// An object in the glTF scene graph, its matrix is relative to its parent
	struct Node
	{
		Node* parent = nullptr;
		std::vector<Node*> children;
		std::vector<Craig::SubMesh*> subMeshes; // not owned, they live in Model::subMeshes
		glm::mat4 matrix = glm::mat4(1.0f);

		glm::mat4 getWorldMatrix() const; // Walks up the parents
		~Node() { for (Node* child : children) delete child; }
	};

	struct Model {
		std::vector<Craig::SubMesh*> subMeshes; // every primitive, flat, for building the vertex/index buffers
		uint32_t subMeshesCount;
		std::string modelPath;

		std::vector<Craig::Node*> nodes; // top level nodes only
		std::vector<Craig::Texture> images; // every glTF image + a 1x1 white fallback at the end
		std::vector<Craig::GltfTexture> textures;
		std::vector<Craig::Material> materials;

		// Falls back to a plain white material if the index is -1 or out of range
		const Craig::Material& getMaterial(int32_t materialIndex) const;
		// Follows texture -> image, gives the white fallback if there isn't one
		Craig::Texture& getMaterialImage(const Craig::Material& material);
		// box around every vertex with the node transforms applied, in the model's own space
		// false if there's no geometry
		// walks every vertex so it's slow, use getBounds() unless the mesh changed
		bool calculateBounds(glm::vec3& min, glm::vec3& max) const;
		// same box, worked out once when the model loads
		bool getBounds(glm::vec3& min, glm::vec3& max) const { min = boundsMin; max = boundsMax; return hasBounds; }
		glm::vec3 boundsMin = glm::vec3(0.0f);
		glm::vec3 boundsMax = glm::vec3(0.0f);
		bool hasBounds = false;
		// Every vertex position with the node transforms applied, in the model's own space (same as calculateBounds)
		void collectPoints(std::vector<glm::vec3>& outPoints) const;
		// ray vs every triangle, ray's in the model's own space (same as calculateBounds)
		// distance is in units of dir, so leave dir unnormalised if you want it to match the caller's space
		bool raycast(const glm::vec3& origin, const glm::vec3& dir, float& outDistance) const;
	};

	

	class ResourceManager {

	public:
		CraigError init(Craig::Renderer* rendererToSet);
		CraigError terminate();

		bool loadModel(std::string modelPath);
		void terminateModels(const vk::Device& device, const VmaAllocator& memoryAllocator);

		Craig::Model& getModel(std::string modelPath) { return m_loadedModels[modelPath]; };
		bool isModelLoaded(const std::string& modelPath) { return m_loadedModels.contains(modelPath); };
		std::unordered_map<std::string, Craig::Model>& getLoadedModels() { return m_loadedModels; };

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
		//Craig::Model m_testModel;
		std::unordered_map<std::string, Craig::Model> m_loadedModels;
	};



}