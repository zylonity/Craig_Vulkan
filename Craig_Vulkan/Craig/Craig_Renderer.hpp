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

		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
		};

		// Encapsulates the Vulkan initialization process
		void InitVulkan();

		// Debugging functions
		void setupDebugMessenger(); // Sets up the Vulkan debug messenger after instance creation
		void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo); // Fills the debug messenger config
		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
			void* userData); // This is the function Vulkan will call to report debug messages

		// Physical device selection functions
		void pickPhysicalDevice(); // Picks a suitable physical device for rendering
		bool isDeviceSuitable(vk::PhysicalDevice device);
		QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);

		void createLogicalDevice(); //Logical device to interact with the physical device


		Window* mp_CurrentWindow = nullptr; // Pointer to the current window

		std::vector<const char*> m_VK_Layers; //Validation layers for debugging

		vk::Instance m_VK_instance; // Vulkan instance for rendering

		vk::ApplicationInfo m_VK_appInfo; // Application information for Vulkan instance creation
		vk::InstanceCreateInfo m_VK_instInfo; // Instance creation information for Vulkan instance

		vk::SurfaceKHR m_VK_surface; // Vulkan surface for rendering (vk::SurfaceKHR wrapper)

		vk::DebugUtilsMessengerEXT m_VK_debugMessenger; // Debug messenger for Vulkan validation layers (vk::DebugUtilsMessengerEXT wrapper)

		vk::PhysicalDevice m_VK_physicalDevice; // Physical device for rendering (vk::PhysicalDevice wrapper)

		vk::Device m_VK_device; // Logical device for rendering (vk::Device wrapper)

		vk::Queue m_VK_graphicsQueue; // Graphics queue for rendering (vk::Queue wrapper)

	};



}