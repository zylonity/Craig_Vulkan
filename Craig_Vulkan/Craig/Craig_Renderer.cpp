#include <cassert>
#include <iostream>

#include "Craig_Renderer.hpp"
#include "Craig_Window.hpp"

CraigError Craig::Renderer::init(Window* CurrentWindowPtr) {

	CraigError ret = CRAIG_SUCCESS;

	// Check if the current window pointer is valid
	assert(CurrentWindowPtr != nullptr && "CurrentWindowPtr is null, cannot initialize Renderer without a valid window pointer.");

	//Pass in the current window pointer (Done in framework)
	mp_CurrentWindow = CurrentWindowPtr; 

	// Ensure that the current window pointer is not null (just to be extra safe)
	assert(mp_CurrentWindow != nullptr && "mp_CurrentWindow is null, somehow didn't get passed to our member variable");

	// Use validation layers if this is a debug build
#if defined(_DEBUG)
	m_VK_Layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // vk::ApplicationInfo allows the programmer to specifiy some basic information about the
    // program, which can be useful for layers and tools to provide more debug information.
    m_VK_appInfo = vk::ApplicationInfo()
        .setPApplicationName(kVK_AppName)
        .setApplicationVersion(kVK_AppVersion)
        .setPEngineName(kVK_EngineName)
        .setEngineVersion(kVK_EngineVersion)
        .setApiVersion(VK_API_VERSION_1_0);

    // vk::InstanceCreateInfo is where the programmer specifies the layers and/or extensions that
    // are needed.
    m_VK_instInfo = vk::InstanceCreateInfo()
        .setFlags(vk::InstanceCreateFlags())
        .setPApplicationInfo(&m_VK_appInfo)
        .setEnabledExtensionCount(static_cast<uint32_t>(mp_CurrentWindow->getExtensionsVector().size()))
        .setPpEnabledExtensionNames(mp_CurrentWindow->getExtensionsVector().data())
        .setEnabledLayerCount(static_cast<uint32_t>(m_VK_Layers.size()))
        .setPpEnabledLayerNames(m_VK_Layers.data());

	//Create the Vulkan instance/Initialize Vulkan
    try {
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        populateDebugMessengerCreateInfo(debugCreateInfo);
        m_VK_instInfo.setPNext(&debugCreateInfo); // Attach debug info to the instance creation

        m_VK_instance = vk::createInstance(m_VK_instInfo);
        setupDebugMessenger(); // Actually enable the messenger
    }
    catch (const std::exception& e) {
        std::string message = std::string("Vulkan instance creation failed: ") + e.what();
		assert(false && message.c_str()); // Technicallllyyy.. undefined behavior cos of the string, but we want to crash here anyways.
    }

    // Create a Vulkan surface for rendering
    VkSurfaceKHR cSurface; // Vulkan surface for rendering
	bool sdlRetBool = SDL_Vulkan_CreateSurface(mp_CurrentWindow->getSDLWindow(), static_cast<VkInstance>(m_VK_instance), &cSurface);
    assert(sdlRetBool && "Could not create a Vulkan surface.");

    m_VK_surface = vk::SurfaceKHR(cSurface);

	return ret;
}

CraigError Craig::Renderer::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::Renderer::terminate() {

	CraigError ret = CRAIG_SUCCESS;

    //Destroy the messenger/debugger
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

    // Clean up.
    m_VK_instance.destroySurfaceKHR(m_VK_surface);
    m_VK_instance.destroy();

	return ret;
}

// This function is called by Vulkan to report debug messages.
// TODO: Maybe switch this to a logger later on, avoiding std::cerr
VKAPI_ATTR VkBool32 VKAPI_CALL Craig::Renderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData) {
    std::cerr << "Validation layer: " << callbackData->pMessage << std::endl;
    return VK_FALSE;
}

// Fills out the configuration for the Vulkan debug messenger.
void Craig::Renderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
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
void Craig::Renderer::setupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo); // Fill with default settings

    // Look up the address of vkCreateDebugUtilsMessengerEXT at runtime
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        m_VK_instance.getProcAddr("vkCreateDebugUtilsMessengerEXT");

    // If the function was loaded successfully, call it to create the messenger
    if (func && func(static_cast<VkInstance>(m_VK_instance), &createInfo, nullptr,
        reinterpret_cast<VkDebugUtilsMessengerEXT*>(&m_VK_debugMessenger)) != VK_SUCCESS) {
        // Throw if something went wrong setting it up
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void Craig::Renderer::pickPhysicalDevice() {

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_VK_instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("No physical devices found with Vulkan support.");
	}

	// Resize the vector to hold the physical devices
	m_VK_devices.resize(deviceCount);
	// Get the list of physical devices
    vkEnumeratePhysicalDevices(m_VK_instance, &deviceCount, m_VK_devices.data());

    for (const auto& device : m_VK_devices) {
        if (isDeviceSuitable(device)) {
            m_VK_physicalDevice = device;
            break;
        }
    }

    if (m_VK_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }


}

bool Craig::Renderer::isDeviceSuitable(VkPhysicalDevice device) {
	//Since we're just starting out, we'll assume all devices are suitable, but I can change this later depending on what I want to do with the engine.
    return true;
}