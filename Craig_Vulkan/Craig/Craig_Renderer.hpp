#pragma once

#include <optional>
#include <vector>
#include <array>
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL_vulkan.h>
#include <glm/glm.hpp>

#include "Craig_Constants.hpp"


namespace Craig {

	//Forward declarations
	class Window;

	class Renderer {

	public:
		CraigError init(Window* CurrentWindowPtr);
		CraigError update();
		CraigError terminate();
	private:

		struct UniformBufferObject {
			glm::mat4 model;
			glm::mat4 view;
			glm::mat4 proj;
		};

		//Essentially (from what I understand) it's the index (or like ID) of WHERE the queue is, 
		// so the queue for graphics rendering could be at place 0, and the queue for presenting could be at place 2
		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
			std::optional<uint32_t> transferFamily;

			bool isComplete() {
				return graphicsFamily.has_value() && presentFamily.has_value();
			}

			bool hasDedicatedTransfer() {
				return graphicsFamily && transferFamily && transferFamily != graphicsFamily;
			}

		};

		struct SwapChainSupportDetails {
			vk::SurfaceCapabilitiesKHR capabilities;
			std::vector<vk::SurfaceFormatKHR> formats;
			std::vector<vk::PresentModeKHR> presentModes;

		};

		// Encapsulates the Vulkan initialization process
		void InitVulkan();
		void setupDebugMessenger(); // Sets up the Vulkan debug messenger after instance creation
		void pickPhysicalDevice(); // Picks a suitable physical device for rendering
		void createLogicalDevice(); //Logical device to interact with the physical device
		void createRenderPass();

		void createDescriptorSetLayout();

		void createGraphicsPipeline();
		void createCommandPool();
		void createCommandBuffers();
		void createSyncObjects();

		void recreateSwapChain();
		void createSwapChain(); //Create double/triple buffer
		void createImageViews();
		void createFrameBuffers();
		void cleanupSwapChain();

		void createVertexBuffer();
		void createIndexBuffer();
		void createUniformBuffers();



		void drawFrame();

		vk::detail::DispatchLoaderStatic onPresentationFail() { return vk::detail::DispatchLoaderStatic{}; };

		// Debugging functions
		void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo); // Fills the debug messenger config
		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
			void* userData); // This is the function Vulkan will call to report debug messages

		// Physical device selection functions
		bool isDeviceSuitable(const vk::PhysicalDevice& device);
		bool checkDeviceExtensionSupport(const vk::PhysicalDevice& device);
		

		QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice& device);
		SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device);

		vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
		vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
		vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

		void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);
		

		const std::vector<const char*> mv_VK_deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME // Required for swapchain support (Like a framebuffer)
		};

		uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);

		void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory);
		void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

		Window* mp_CurrentWindow = nullptr; // Pointer to the current window

		std::vector<const char*> mv_VK_Layers; //Validation layers for debugging

		vk::Instance m_VK_instance; // Vulkan instance for rendering

		vk::ApplicationInfo m_VK_appInfo; // Application information for Vulkan instance creation
		vk::InstanceCreateInfo m_VK_instInfo; // Instance creation information for Vulkan instance

		vk::DebugUtilsMessengerEXT m_VK_debugMessenger; // Debug messenger for Vulkan validation layers

		vk::SurfaceKHR m_VK_surface; // Vulkan surface for rendering (vk::SurfaceKHR wrapper)
		vk::PhysicalDevice m_VK_physicalDevice; // Physical device for rendering
		vk::Device m_VK_device; // Logical device for rendering

		vk::Queue m_VK_graphicsQueue; // Graphics queue for rendering
		vk::Queue m_VK_presentationQueue; // Presentation queue for rendering
		vk::Queue m_VK_transferQueue;

		// The swapchain owns a rotating set of images that we render to and present to the screen.
		// Think of it like a queue of framebuffers managed by the GPU/display system, but framebuffers are the complete image, after applying the imageview
		vk::SwapchainKHR m_VK_swapChain; 
		std::vector<vk::Image> mv_VK_swapChainImages;
		vk::Format m_VK_swapChainImageFormat;
		vk::Extent2D m_VK_swapChainExtent;

		// Image views describe how we access each swapchain image, like treating raw images as 2D textures.
		std::vector<vk::ImageView> mv_VK_swapChainImageViews;

		vk::ShaderModule m_VK_vertShaderModule;
		vk::ShaderModule m_VK_fragShaderModule;

		vk::RenderPass m_VK_renderPass;
		vk::DescriptorSetLayout m_VK_descriptorSetLayout;
		vk::PipelineLayout m_VK_pipelineLayout;
		vk::Pipeline m_VK_graphicsPipeline;
		

		std::vector<vk::Framebuffer> mv_VK_swapChainFramebuffers;

		vk::CommandPool m_VK_commandPool;
		vk::CommandPool m_VK_transferCommandPool;

		std::vector <vk::CommandBuffer> mv_VK_commandBuffers;

		uint32_t m_currentFrame = 0;
		std::vector<vk::Semaphore> mv_VK_imageAvailableSemaphores;
		std::vector<vk::Semaphore> mv_VK_renderFinishedSemaphores;
		std::vector<vk::Fence> mv_VK_inFlightFences;

		vk::Buffer m_VK_vertexBuffer;
		vk::DeviceMemory m_VK_vertexBufferMemory;
		vk::Buffer m_VK_indexBuffer;
		vk::DeviceMemory m_VK_indexBufferMemory;

		std::vector<vk::Buffer> mv_VK_uniformBuffers;
		std::vector<vk::DeviceMemory> mv_VK_uniformBuffersMemory;
		std::vector<void*> mv_VK_uniformBuffersMapped;

#if defined(IMGUI_ENABLED)
		void InitImgui();
		void createImguiDescriptorPool();

		vk::DescriptorPool m_VK_imguiDescriptorPool;
#endif

		//Vertex buffer stuff
		struct Vertex {
			glm::vec2 m_pos;
			glm::vec3 m_color;

			static vk::VertexInputBindingDescription getBindingDescription(); //A vertex binding describes at which rate to load data from memory throughout the vertices. It specifies the number of bytes between data entries and whether to move to the next data entry after each vertex or after each instance.
			static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions(); //We have two attributes, position and color, so we need two attribute description structs.

		};

		const std::vector<Vertex> m_vertices = {
			{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
			{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
			{{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
		};

		const std::vector<uint16_t> m_indices = {
			0, 1, 2, 2, 3, 0
		};

	};



}