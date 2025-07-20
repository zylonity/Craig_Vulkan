#pragma once

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

		void setupDebugMessenger(); // Sets up the Vulkan debug messenger after instance creation
		void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo); // Fills the debug messenger config
		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
			void* userData); // This is the function Vulkan will call to report debug messages

		void pickPhysicalDevice(); // Picks a suitable physical device for rendering
		bool isDeviceSuitable(VkPhysicalDevice device);

		Window* mp_CurrentWindow = nullptr; // Pointer to the current window

		std::vector<const char*> m_VK_Layers; //Validation layers for debugging

		vk::Instance m_VK_instance; // Vulkan instance for rendering

		vk::ApplicationInfo m_VK_appInfo; // Application information for Vulkan instance creation
		vk::InstanceCreateInfo m_VK_instInfo; // Instance creation information for Vulkan instance

		vk::SurfaceKHR m_VK_surface; // Vulkan surface for rendering (vk::SurfaceKHR wrapper)

		vk::DebugUtilsMessengerEXT m_VK_debugMessenger; // Debug messenger for Vulkan validation layers (vk::DebugUtilsMessengerEXT wrapper)

		VkPhysicalDevice m_VK_physicalDevice; // Physical device for Vulkan rendering (VkPhysicalDevice handle)
		std::vector<VkPhysicalDevice> m_VK_devices;

	};



}