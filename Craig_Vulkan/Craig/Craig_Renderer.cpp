#include <cassert>
#include <iostream>
#include <set>

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
    mp_CurrentWindow->getExtensionsVector().push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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

		m_VK_instance = vk::createInstance(m_VK_instInfo); //Now that we have the instance created, we can initialize Vulkan

        // Create a Vulkan surface for rendering
        VkSurfaceKHR cSurface; // Vulkan surface for rendering
        bool sdlRetBool = SDL_Vulkan_CreateSurface(mp_CurrentWindow->getSDLWindow(), static_cast<VkInstance>(m_VK_instance), &cSurface);
        assert(sdlRetBool && "Could not create a Vulkan surface.");

        m_VK_surface = vk::SurfaceKHR(cSurface);

        InitVulkan();
    }
    catch (const std::exception& e) {
        std::string message = std::string("Vulkan instance creation failed: ") + e.what();
		assert(false && message.c_str()); // Technicallllyyy.. undefined behavior cos of the string, but we want to crash here anyways.
    }


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

    m_VK_device.destroy();

    // Clean up.
    m_VK_instance.destroySurfaceKHR(m_VK_surface);
    m_VK_instance.destroy();

	return ret;
}

void Craig::Renderer::InitVulkan() {
    
    setupDebugMessenger(); // Actually enable the messenger
	pickPhysicalDevice(); // Pick a suitable physical device for rendering
	createLogicalDevice(); // Create a logical device to interact with the physical device
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


    auto devices = m_VK_instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("No Vulkan-compatible GPUs found.");
    }

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            m_VK_physicalDevice = device;
            break;
        }
    }

    if (m_VK_physicalDevice) {
        vk::PhysicalDeviceProperties props = m_VK_physicalDevice.getProperties();
		printf("\nFound GPU: %s\n", props.deviceName);
    }
    else {
        throw std::runtime_error("failed to find a suitable GPU!");
    }


}

//From the tutorial:
/* It has been briefly touched upon before that almost every operation in Vulkan, anything from drawing to uploading textures, 
requires commands to be submitted to a queue. There are different types of queues that originate from different queue families 
and each family of queues allows only a subset of commands. For example, there could be a queue family that only allows processing 
of compute commands or one that only allows memory transfer related commands.*/

Craig::Renderer::QueueFamilyIndices Craig::Renderer::findQueueFamilies(const vk::PhysicalDevice& device) {
    QueueFamilyIndices indices;
    // Logic to find queue family indices to populate struct with

    auto queueFamilies = device.getQueueFamilyProperties();


	//Find at least one queue family that supports graphics operations
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        if(device.getSurfaceSupportKHR(i, m_VK_surface)) {
            indices.presentFamily = i; // If the queue family supports presentation to the surface, set the present family
		}

        if (indices.isComplete()) {
			break; // If we already found a suitable family, no need to keep searching
        }

        i++;
    }

    return indices;
}

bool Craig::Renderer::isDeviceSuitable(const vk::PhysicalDevice& device) {
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    return indices.isComplete() && extensionsSupported;
}

//Here we get the list of available device extensions, and check if the required ones are present
//by removing them from a set and checking if the set is empty at the end.
bool Craig::Renderer::checkDeviceExtensionSupport(const vk::PhysicalDevice& device) {

    std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();

	std::set<std::string> requiredExtensions(m_deviceExtensions.begin(), m_deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void Craig::Renderer::createLogicalDevice() {
    // Query the queue families that support graphics and presentation
    QueueFamilyIndices indices = findQueueFamilies(m_VK_physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    // Use a set to avoid duplicating queue create info if graphics == presentation
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f; // Priority for the queue(s) we are creating (range: 0.0 to 1.0)

    // Create a vk::DeviceQueueCreateInfo for each unique queue family
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo = vk::DeviceQueueCreateInfo()
            .setQueueFamilyIndex(queueFamily)
            .setQueueCount(1)
            .setPQueuePriorities(&queuePriority);

        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::PhysicalDeviceFeatures deviceFeatures; // Enable desired features (none yet, placeholder)

    // Fill in device creation info with queue setup and feature requirements
    vk::DeviceCreateInfo createInfo = vk::DeviceCreateInfo()
        .setQueueCreateInfos(queueCreateInfos)
        .setPEnabledFeatures(&deviceFeatures)
        .setPEnabledExtensionNames(m_deviceExtensions);

    // Create the logical device for the selected physical device
    try {
        m_VK_device = m_VK_physicalDevice.createDevice(createInfo);
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Failed to create logical device: " + std::string(e.what()));
    }

    // Retrieve the queue handles for rendering and presentation
    m_VK_graphicsQueue = m_VK_device.getQueue(indices.graphicsFamily.value(), 0);
    m_VK_presentationQueue = m_VK_device.getQueue(indices.presentFamily.value(), 0);
}