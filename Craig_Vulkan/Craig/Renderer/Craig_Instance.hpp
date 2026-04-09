#pragma once
#include <vulkan/vulkan.hpp>


#include "Craig/Craig_Constants.hpp"


namespace Craig {
	class Window; //Forward declaration

	class Instance {
	public:
		//All the stuff we need to pass to the RenderingAttachments from the renderer
		struct InstanceInitInfo
		{
			std::vector<const char*> validationLayerVector;
			Window* p_Window = nullptr;
		};

		CraigError init(const InstanceInitInfo& info);
		CraigError update();
		CraigError terminate();

		const vk::Instance getVkInstance() const { return m_VK_instance; }
		const vk::SurfaceKHR getVkSurface() const { return m_VK_surface; }

	private:

		void setupDebugMessenger();

		// Debug / utilities
		void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
			VkDebugUtilsMessageSeverityFlagBitsEXT severity,
			VkDebugUtilsMessageTypeFlagsEXT type,
			const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
			void* userData);

		// Vulkan instance / device
		vk::Instance               m_VK_instance;
		vk::DebugUtilsMessengerEXT m_VK_debugMessenger;

		vk::SurfaceKHR       m_VK_surface;

		std::vector<const char*> mv_ITNC_Layers; // Validation layers
		Window* mp_CurrentWindow = nullptr;


	};



}