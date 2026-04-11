#include <SDL2/SDL_vulkan.h>

#include "Craig_Instance.hpp"
#include "../Craig_Window.hpp"

#include <iostream>
#include <ostream>

CraigError Craig::Instance::init(const InstanceInitInfo& info) {

	CraigError ret = CRAIG_SUCCESS;

	mv_ITNC_Layers = info.validationLayerVector;
	mp_CurrentWindow = info.p_Window;


	// vk::ApplicationInfo allows the programmer to specifiy some basic information about the
	// program, which can be useful for layers and tools to provide more debug information.
	vk::ApplicationInfo m_VK_appInfo = vk::ApplicationInfo()
		.setPApplicationName(kVK_AppName)
		.setApplicationVersion(kVK_AppVersion)
		.setPEngineName(kVK_EngineName)
		.setEngineVersion(kVK_EngineVersion)
		.setApiVersion(VK_API_VERSION_1_4);

	// vk::InstanceCreateInfo is where the programmer specifies the layers and/or extensions that
	// are needed.
	vk::InstanceCreateInfo m_VK_instInfo = vk::InstanceCreateInfo()
#if defined(_WIN32) || defined(__linux__)
		.setFlags(vk::InstanceCreateFlags())
#elif defined(__APPLE__)
		.setFlags(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR)
#endif
		.setPApplicationInfo(&m_VK_appInfo)
		.setEnabledExtensionCount(static_cast<uint32_t>(mp_CurrentWindow->getExtensionsVector().size()))
		.setPpEnabledExtensionNames(mp_CurrentWindow->getExtensionsVector().data())
		.setEnabledLayerCount(static_cast<uint32_t>(mv_ITNC_Layers.size()))
		.setPpEnabledLayerNames(mv_ITNC_Layers.data());

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	populateDebugMessengerCreateInfo(debugCreateInfo);

	//featuresInfo.pNext = &debugCreateInfo;
	m_VK_instInfo.setPNext(&debugCreateInfo);

	m_VK_instance = vk::createInstance(m_VK_instInfo); //Now that we have the instance created, we can initialize Vulkan

	// Create a Vulkan surface for rendering
	VkSurfaceKHR cSurface; // Vulkan surface for rendering
	bool sdlRetBool = SDL_Vulkan_CreateSurface(mp_CurrentWindow->getSDLWindow(), static_cast<VkInstance>(m_VK_instance), &cSurface);
	assert(sdlRetBool && "Could not create a Vulkan surface.");

	m_VK_surface = vk::SurfaceKHR(cSurface);

#if defined(_DEBUG)
	setupDebugMessenger();
#endif
	return ret;
}

// This function is called by Vulkan to report debug messages.
// TODO: Maybe switch this to a logger later on, avoiding std::cerr
VKAPI_ATTR VkBool32 VKAPI_CALL Craig::Instance::debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
	void* userData) {
	std::cerr << "Validation layer: " << callbackData->pMessage << std::endl;
	return VK_FALSE;
}

// Fills out the configuration for the Vulkan debug messenger.
void Craig::Instance::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
	createInfo = {}; // Zero out the struct to avoid uninitialized garbage

	// Tell Vulkan this is a debug messenger creation struct
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

	// Choose the severity levels of messages you want to receive
	createInfo.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | // Informational messages (can be spammy)
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | // Potential issues, not always fatal
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;    // Serious problems that likely break things

	// Specify which types of messages you care about
	createInfo.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |     // Miscellaneous messages
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |  // Validation layer warnings/errors
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;  // Performance issues (e.g. suboptimal usage)

	// Provide the callback function to handle these messages
	createInfo.pfnUserCallback = debugCallback;
}

// Registers the debug messenger with Vulkan using the previously filled-in struct
void Craig::Instance::setupDebugMessenger() {
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	populateDebugMessengerCreateInfo(createInfo); // Fill with default settings

	// Look up the address of vkCreateDebugUtilsMessengerEXT at runtime
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
		m_VK_instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");

	// If the function was loaded successfully, call it to create the messenger
	if (!func || func(static_cast<VkInstance>(m_VK_instance), &createInfo, nullptr,
		reinterpret_cast<VkDebugUtilsMessengerEXT*>(&m_VK_debugMessenger)) != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
		}
}

CraigError Craig::Instance::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	m_VK_instance.destroySurfaceKHR(m_VK_surface);

	//Destroy the messenger/debugger
	//Needs to be done before destroying the instance
#if defined(_DEBUG)
	if (m_VK_debugMessenger) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
			m_VK_instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT");

		if (func) {
			func(static_cast<VkInstance>(m_VK_instance),
				static_cast<VkDebugUtilsMessengerEXT>(m_VK_debugMessenger), nullptr);
		}
	}
#endif

	m_VK_instance.destroy();

	return ret;
}


