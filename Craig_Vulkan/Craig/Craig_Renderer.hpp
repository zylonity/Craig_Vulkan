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

namespace Craig {

	//Forward declarations
	class Window;
	class SceneManager;

	class Renderer {

	public:
		CraigError init(Window* CurrentWindowPtr, SceneManager* sceneManagerPtr);
		CraigError update(const float& deltaTime);
		CraigError terminate();

		bool& getVSyncState() { return m_vsync; };
		void refreshSwapChain() { recreateSwapChain(); };
	
		void createTextureImage2(const uint8_t* pixels, int texWidth, int texHeight, int texChannels);

		const uint32_t& getMaxLOD() const { return m_VK_mipLevels; };
		void updateMinLOD(int minLOD);

		const uint32_t& getMaxSamplingLevel() const { return m_MaxSamplingLevel; };
		void updateSamplingLevel(int levelToSet);

	private:
		
		struct UniformBufferObject {
			glm::mat4 model;
			glm::mat4 view;
			glm::mat4 proj;
		};

		// Queue family indices (graphics/present/transfer)
		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
			std::optional<uint32_t> transferFamily;

			bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
			bool hasDedicatedTransfer() { return graphicsFamily && transferFamily && transferFamily != graphicsFamily; }
		};

		// Swapchain support query results
		struct SwapChainSupportDetails {
			vk::SurfaceCapabilitiesKHR capabilities;
			std::vector<vk::SurfaceFormatKHR> formats;
			std::vector<vk::PresentModeKHR> presentModes;
		};

		
		// Core init / teardown
		void InitVulkan();                 // Full Vulkan bring-up
		void setupDebugMessenger();        // Validation callback
		void pickPhysicalDevice();         // Choose GPU
		void createLogicalDevice();        // Create vk::Device + queues
		void initVMA();                    // VMA allocator setup

		
		// Swapchain + framebuffer resources
		void recreateSwapChain();          // Swapchain-only recreation
		void recreateSwapChainFull();      // Swapchain + pipeline + imgui recreation
		void createSwapChain();            // Create swapchain images
		void createImageViews();           // Create swapchain image views
		void cleanupSwapChain();           // Destroy swapchain-related objects

		// MSAA + depth attachments
		void createColourResources();
		void createDepthResources();

		
		// Pipeline / descriptors
		void createDescriptorSetLayout();
		void createGraphicsPipeline();
		void cleanupGraphicsPipeline();

		void createDescriptorPool();
		void createDescriptorSets();
		void updateDescriptorSets();

		
		// Buffers / per-frame data
		void createVertexBuffer();
		void createIndexBuffer();
		void createUniformBuffers();
		void updateUniformBuffer(uint32_t currentImage, const float& deltaTime);

		
		// Command submission + sync
		void createCommandPool();
		void createCommandBuffers();
		void createSyncObjects();

		void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);
		void drawFrame(const float& deltaTime);

		// One-off command helpers (transfer/GFX)
		vk::CommandBuffer buffer_beginSingleTimeCommands();
		void buffer_endSingleTimeCommands(vk::CommandBuffer commandBuffer);
		vk::CommandBuffer buffer_beginSingleTimeCommandsGFX();     // Uses graphics queue
		void buffer_endSingleTimeCommandsGFX(vk::CommandBuffer commandBuffer);

		
		// Images / textures helpers
		void createTextureImageView();
		void createTextureSampler();
		void terminateSampler();

		void createBufferVMA(vk::DeviceSize size,
			vk::BufferUsageFlags usage,
			const VmaAllocationCreateInfo& aci,
			vk::Buffer& buffer,
			VmaAllocation& alloc,
			VmaAllocationInfo* outInfo = nullptr);

		void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

		void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
			vk::SampleCountFlagBits numSamples,
			vk::Format format,
			vk::ImageTiling tiling,
			vk::ImageUsageFlags usage,
			vk::MemoryPropertyFlags properties,
			vk::Image& image,
			VmaAllocation& allocation);

		void transitionImageLayout(vk::Image image, vk::Format format,
			vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
			bool useTransferQueue = true, uint32_t mipLevels = 1);

		void copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

		void generateMipMaps(vk::Image image, vk::Format format,
			int32_t texWidth, int32_t texHeight,
			uint32_t mipLevels, bool useTransferQueue);

		void transitionSwapImage(vk::CommandBuffer cmd, vk::Image img,
			vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

		vk::ImageView createImageView(vk::Image image, vk::Format format,
			vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);

		
		// Device / swapchain queries
		bool isDeviceSuitable(const vk::PhysicalDevice& device);
		bool checkDeviceExtensionSupport(const vk::PhysicalDevice& device);

		QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice& device);
		SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device);

		vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
		vk::PresentModeKHR   chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
		vk::Extent2D         chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

		uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

		// MSAA + formats
		vk::SampleCountFlagBits getMaxUsableSampleCount();
		vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates,
			vk::ImageTiling tiling, vk::FormatFeatureFlags features);
		vk::Format findDepthFormat();
		bool hasStencilComponent(vk::Format format);

		
		// Debug / utilities
		void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
			void* userData);

		
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

		Craig::Camera m_camera = Craig::Camera();

		
		// Vulkan instance / device
		vk::Instance               m_VK_instance;
		vk::ApplicationInfo        m_VK_appInfo;
		vk::InstanceCreateInfo     m_VK_instInfo;
		vk::DebugUtilsMessengerEXT m_VK_debugMessenger;

		vk::SurfaceKHR       m_VK_surface;
		vk::PhysicalDevice   m_VK_physicalDevice;
		vk::Device           m_VK_device;

		vk::Queue m_VK_graphicsQueue;
		vk::Queue m_VK_presentationQueue;
		vk::Queue m_VK_transferQueue;

		
		// Swapchain
		vk::SwapchainKHR           m_VK_swapChain;
		std::vector<vk::Image>     mv_VK_swapChainImages;
		std::vector<vk::ImageView> mv_VK_swapChainImageViews;
		vk::Format                 m_VK_swapChainImageFormat;
		vk::Extent2D               m_VK_swapChainExtent;

		
		// Shaders / pipeline
		vk::ShaderModule       m_VK_vertShaderModule;
		vk::ShaderModule       m_VK_fragShaderModule;

		vk::DescriptorSetLayout m_VK_descriptorSetLayout;
		vk::PipelineLayout      m_VK_pipelineLayout;
		vk::Pipeline            m_VK_graphicsPipeline;

		
		// Commands
		vk::CommandPool                m_VK_commandPool;
		vk::CommandPool                m_VK_transferCommandPool;
		std::vector<vk::CommandBuffer> mv_VK_commandBuffers;

		
		// Sync
		uint32_t m_currentFrame = 0;

		std::vector<vk::Semaphore> mv_VK_imageAvailableSemaphores;
		std::vector<vk::Semaphore> mv_VK_renderFinishedSemaphores;

		// Timeline semaphore (optional sync style)
		vk::Semaphore m_VK_timelineSemaphore;
		uint64_t      m_sempahoreTimelineValue = 0;

		
		// Geometry buffers
		vk::Buffer     m_VK_vertexBuffer;
		VmaAllocation  m_VMA_vertexAllocation;

		vk::Buffer     m_VK_indexBuffer;
		VmaAllocation  m_VMA_indexAllocation;

		
		// Uniforms / descriptors
		std::vector<vk::Buffer>    mv_VK_uniformBuffers;
		std::vector<VmaAllocation> mv_VK_uniformBuffersAllocations;
		std::vector<void*>        mv_VK_uniformBuffersMapped;

		vk::DescriptorPool              m_VK_descriptorPool;
		std::vector<vk::DescriptorSet>  mv_VK_descriptorSets;

		
		// MSAA / colour / depth
		vk::SampleCountFlagBits m_VK_msaaSamples = vk::SampleCountFlagBits::e1;

		uint32_t m_MaxSamplingLevel = 0;   // Max sampling level
		uint32_t m_minLODLevel = 0;        // User-selected min LOD clamp

		vk::Image      m_VK_colourImage;
		vk::ImageView  m_VK_colourImageView;
		VmaAllocation  m_VMA_colourImageAllocation;

		vk::Image      m_VK_depthImage;
		vk::ImageView  m_VK_depthImageView;
		VmaAllocation  m_VMA_depthImageAllocation;

		
		// Texture
		uint32_t      m_VK_mipLevels = 0;

		vk::Image     m_VK_textureImage;
		VmaAllocation m_VMA_textureImageAllocation;

		vk::ImageView m_VK_textureImageView;
		vk::Sampler   m_VK_textureSampler;

		
		// VMA allocator / pools
		VmaAllocator m_VMA_allocator = VK_NULL_HANDLE;
		VmaPool      m_VMA_smallItemsPool = VK_NULL_HANDLE;

		// Misc
		vk::Extent2D m_VK_currentExtent;
		bool m_vsync = true;

		
		// ImGui
#if defined(IMGUI_ENABLED)
		void InitImgui();
		void createImguiDescriptorPool();
		vk::DescriptorPool m_VK_imguiDescriptorPool;
#endif
	};

}