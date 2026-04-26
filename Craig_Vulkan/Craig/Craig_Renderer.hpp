#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <optional>
#include <vector>
#include <array>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_beta.h>
#include <SDL2/SDL_vulkan.h>
#include "../External/vk_mem_alloc.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Craig_Constants.hpp"
#include "Craig_Camera.hpp"
#include "Craig_ResourceManager.hpp"
#include "Renderer/Craig_CommandManager.hpp"
#include "Renderer/Craig_Swapchain.hpp"
#include "Renderer/Craig_Device.hpp"
#include "Renderer/Craig_Instance.hpp"
#include "Renderer/Craig_Pipeline.hpp"
#include "Renderer/Craig_RenderingAttachments.hpp"
#include "Renderer/Craig_SyncManager.hpp"

namespace Craig {

	//Forward declarations
	class Window;
	class SceneManager;
	class GameObject;

	class Renderer {

	public:
		CraigError init(Window* CurrentWindowPtr, SceneManager* sceneManagerPtr);
		CraigError update(const float& deltaTime);
		CraigError terminate();

		bool& getVSyncState() { return m_swapChain.m_vsyncEnabled; };
		void refreshSwapChain() { recreateSwapChain(); };
		void createTextureImage2(const uint8_t* pixels, int texWidth, int texHeight, int texChannels, Texture* outTexture);

		void updateMinLOD(int minLOD);

		//const uint32_t& getMaxSamplingLevel() const { return m_MaxSamplingLevel; };
		void updateSamplingLevel(int levelToSet);

		RenderingAttachments getRenderingAttachments() {return m_renderingAttachments; };

		const glm::vec2 getWindowSize() const;

		void deleteGameObject(Craig::GameObject* gameObject);
		CraigError newGameObject(std::string objectName, std::string modelPath, glm::vec3 position);

	private:
		struct PerObjectData {
			glm::mat4 model;
		};

		struct CameraData {
			glm::mat4 view;
			glm::mat4 proj;
		};

		struct LightData
		{
			glm::vec3 lightDir;
			glm::vec3 lightColour;
			glm::vec3 ambientColour;
		};

		// struct UniformBufferObject {
		// 	glm::mat4 model;
		// 	glm::mat4 view;
		// 	glm::mat4 proj;
		// };
		
		// Core init / teardown
		void InitVulkan();                 // Full Vulkan bring-up
		
		// Swapchain + framebuffer resources
		void recreateSwapChain();          // Swapchain-only recreation
		void recreateSwapChainFull();      // Swapchain + pipeline + imgui recreation

		void createDescriptorPool();
		void createDescriptorSets();
		void updateDescriptorSets();

		
		// Buffers / per-frame data
		void createVertexBuffer();
		void createIndexBuffer();
		//void createUniformBuffers();
		void createUniformBuffers();
		void updateUniformBuffer(uint32_t currentImage, const float& deltaTime);

		
		// Command submission + sync
		//void createSyncObjects();

		void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);
		void drawFrame(const float& deltaTime);

		
		// Images / textures helpers
		//void createTextureImageView();
		void createTextureSampler();
		void terminateSampler();


		
		// Extensions / layers
		const std::vector<const char*> mv_VK_deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		#if defined(__APPLE__)
			, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
		#endif
		};

		std::vector<const char*> mv_VK_Layers; // Validation layers

		
		// Engine-facing state
		SceneManager* mp_SceneManager = nullptr;
		Window* mp_CurrentWindow = nullptr;

		Craig::Instance m_instance; //Contains vulkan instance and debugging stuff
		Craig::Device m_Devices; //Contains physical and logical device

		// Swapchain
		Craig::Swapchain m_swapChain;
		
		// Shaders / pipeline
		Craig::Pipeline m_pipeline;

		
		// Commands
		Craig::CommandManager m_commandManager;

		
		// Sync
		Craig::SyncManager m_syncManager;

		
		// Geometry buffers
		vk::Buffer     m_VK_vertexBuffer;
		VmaAllocation  m_VMA_vertexAllocation;

		vk::Buffer     m_VK_indexBuffer;
		VmaAllocation  m_VMA_indexAllocation;

		
		// Uniforms / descriptors
		std::vector<vk::Buffer>    mv_VK_storageBuffers;
		std::vector<VmaAllocation> mv_VK_storageBuffersAllocations;
		std::vector<void*>        mv_VK_storageBuffersMapped;

		std::vector<vk::Buffer> mv_viewProjUboBuffer;
		std::vector<VmaAllocation> mv_viewProjUboAllocation;
		std::vector<void*> mv_viewProjUboMap;

		std::vector<vk::Buffer> mv_lightUboBuffer;
		std::vector<VmaAllocation> mv_lightUboAllocation;
		std::vector<void*> mv_lightUboMap;

		vk::DescriptorPool              m_VK_descriptorPool;
		std::vector<vk::DescriptorSet>	mv_VK_perFrameDescriptorSet;

		std::unordered_map<GameObject*, vk::DescriptorSet>  mMap_GameObjectToDescriptorSet;

		uint32_t m_minLODLevel = 0;        // User-selected min LOD clamp

		RenderingAttachments m_renderingAttachments; //Contains stuff for MSAA, vsync and mipmap levels
		
		// Texture
		vk::Sampler   m_VK_textureSampler;

		// VMA allocator / pools
		VmaPool      m_VMA_smallItemsPool = VK_NULL_HANDLE;

		
		// ImGui
#if defined(IMGUI_ENABLED)
		void InitImgui();
		void createImguiDescriptorPool();
		vk::DescriptorPool m_VK_imguiDescriptorPool;
#endif
	};

}
