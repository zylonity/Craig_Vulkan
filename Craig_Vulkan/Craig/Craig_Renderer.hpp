#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <SDL2/SDL_vulkan.h>

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

		//Essentially (from what I understand) it's the index (or like ID) of WHERE the queue is, 
		// so the queue for graphics rendering could be at place 0, and the queue for presenting could be at place 2
		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;

			bool isComplete() {
				return graphicsFamily.has_value() && presentFamily.has_value();
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
		void createSwapChain(); //Create double/triple buffer
		void createImageViews();

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


		void createGraphicsPipeline();
		

		const std::vector<const char*> m_VK_deviceExtensions = {
			VK_KHR_SWAPCHAIN_EXTENSION_NAME // Required for swapchain support (Like a framebuffer)
		};

		Window* mp_CurrentWindow = nullptr; // Pointer to the current window

		std::vector<const char*> m_VK_Layers; //Validation layers for debugging

		vk::Instance m_VK_instance; // Vulkan instance for rendering

		vk::ApplicationInfo m_VK_appInfo; // Application information for Vulkan instance creation
		vk::InstanceCreateInfo m_VK_instInfo; // Instance creation information for Vulkan instance

		vk::DebugUtilsMessengerEXT m_VK_debugMessenger; // Debug messenger for Vulkan validation layers

		vk::SurfaceKHR m_VK_surface; // Vulkan surface for rendering (vk::SurfaceKHR wrapper)
		vk::PhysicalDevice m_VK_physicalDevice; // Physical device for rendering
		vk::Device m_VK_device; // Logical device for rendering

		vk::Queue m_VK_graphicsQueue; // Graphics queue for rendering
		vk::Queue m_VK_presentationQueue; // Presentation queue for rendering

		vk::SwapchainKHR m_VK_swapChain; 
		std::vector<vk::Image> m_VK_swapChainImages;
		vk::Format m_VK_swapChainImageFormat;
		vk::Extent2D m_VK_swapChainExtent;

		std::vector<vk::ImageView> m_VK_swapChainImageViews;

		vk::ShaderModule m_VK_vertShaderModule;
		vk::ShaderModule m_VK_fragShaderModule;

		vk::PipelineLayout m_VK_pipelineLayout;

	};



}