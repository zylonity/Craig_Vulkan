#include <cassert>
#include <iostream>
#include <set>
#include <algorithm>

#include "Craig_Renderer.hpp"
#include "Craig_Window.hpp"
#include "Craig_ShaderCompilation.hpp"

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
        assert(("Vulkan instance creation failed: " + std::string(e.what())).c_str());
    }


	return ret;
}

CraigError Craig::Renderer::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::Renderer::terminate() {

	CraigError ret = CRAIG_SUCCESS;

    m_VK_device.destroyShaderModule(vertShaderModule);
    m_VK_device.destroyShaderModule(fragShaderModule);

    for (auto imageView : m_VK_swapChainImageViews) {
        m_VK_device.destroyImageView(imageView);
    }


    m_VK_device.destroySwapchainKHR(m_VK_swapChain);

    m_VK_device.destroy();

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

void Craig::Renderer::InitVulkan() {
    
    setupDebugMessenger(); // Actually enable the messenger
	pickPhysicalDevice(); // Pick a suitable physical device for rendering
	createLogicalDevice(); // Create a logical device to interact with the physical device
    createSwapChain();
    createImageViews();

    createGraphicsPipeline();
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

Craig::Renderer::SwapChainSupportDetails Craig::Renderer::querySwapChainSupport(const vk::PhysicalDevice& device) {
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(m_VK_surface);
    details.formats = device.getSurfaceFormatsKHR(m_VK_surface);
    details.presentModes = device.getSurfacePresentModesKHR(m_VK_surface);


    return details;
}

bool Craig::Renderer::isDeviceSuitable(const vk::PhysicalDevice& device) {
    QueueFamilyIndices indices = findQueueFamilies(device); //Check gfx device can render and present to the screen

    bool extensionsSupported = checkDeviceExtensionSupport(device); //Check it supports extensions, especifically the swapchain extension (AKA double buffering)

    bool swapChainAdequate = false; 
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device); //Check the swapchaine extension it has is actually what we want for this
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    printf("Found graphics and presentation indices: %s\n", indices.isComplete() ? "True" : "False");
    printf("Extensions (Like swapchain/double buffers) are supported: %s\n", extensionsSupported ? "True" : "False");
    printf("The swapchain extension is adequate for our use: %s\n", swapChainAdequate ? "True" : "False");

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

//Here we get the list of available device extensions, and check if the required ones are present
//by removing them from a set and checking if the set is empty at the end.
bool Craig::Renderer::checkDeviceExtensionSupport(const vk::PhysicalDevice& device) {

    std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();

	std::set<std::string> requiredExtensions(m_VK_deviceExtensions.begin(), m_VK_deviceExtensions.end());

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
        .setPEnabledExtensionNames(m_VK_deviceExtensions);

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
vk::SurfaceFormatKHR Craig::Renderer::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {

    //Check the colour format and colour space are correct
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) { //TODO: Expose to the class and allow for switching with imgui later.
            return availableFormat;
        }
    }

    return availableFormats[0];
}

vk::PresentModeKHR Craig::Renderer::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {


    //VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
    //VK_PRESENT_MODE_FIFO_KHR : The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered images at the back of the queue.If the queue is full then the program has to wait.This is most similar to vertical sync as found in modern games.The moment that the display is refreshed is known as "vertical blank".
    //VK_PRESENT_MODE_FIFO_RELAXED_KHR : This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank.Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives.This may result in visible tearing.
    //VK_PRESENT_MODE_MAILBOX_KHR : This is another variation of the second mode.Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones.This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync.This is commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.

    //TODO: I want to be able to enable/disable Vsync later, so we expose this to the class and use accessors w/ imgui later
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            return availablePresentMode;
        }
    }


    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Craig::Renderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    else {
        //Rendering resolution, basically.
        int width, height;
        SDL_Vulkan_GetDrawableSize(mp_CurrentWindow->getSDLWindow(), &width, &height);

        vk::Extent2D actualExtent;
        actualExtent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }


}

void Craig::Renderer::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_VK_physicalDevice);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; //Ammount of images we want for buffering, min is probably 2, so tripple buffering

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    printf("Creating draw buffer/swap chain with %i images\n", imageCount);

    vk::SwapchainCreateInfoKHR createInfo;
    createInfo
        .setSurface(m_VK_surface)
        .setMinImageCount(imageCount)
        .setImageFormat(surfaceFormat.format)
        .setImageColorSpace(surfaceFormat.colorSpace)
        .setImageExtent(extent)
        .setImageArrayLayers(1) //"always 1 unless you are developing a stereoscopic 3D application"
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

    QueueFamilyIndices indices = findQueueFamilies(m_VK_physicalDevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    //In case we have separate graphics and presentation queues.
    /*According to vulkan-tutorial.com
    VK_SHARING_MODE_EXCLUSIVE: An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family.
    VK_SHARING_MODE_CONCURRENT: Images can be used across multiple queue families without explicit ownership transfers.
    */
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndexCount(2)
            .setPQueueFamilyIndices(queueFamilyIndices);
    }
    else {
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    //We can transform the image here (like a 90 degree clockwise rotation or horizontal flip.)
    //Since we don't want any, we just specify the current transformation applied (which should be none)
    createInfo.setPreTransform(swapChainSupport.capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque) //Blending with other windows in the window system (will mostly always be opaque)
        .setPresentMode(presentMode)
        .setClipped(VK_TRUE) //The GPU won't render pixels that are obscured (by other windows, for example)  it also means we can't trust the data in the pixels since they might've not rendered.
        .setOldSwapchain(VK_NULL_HANDLE);


    try {
        m_VK_swapChain = m_VK_device.createSwapchainKHR(createInfo);
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to create swap chain!");
    }

    m_VK_swapChainImages = m_VK_device.getSwapchainImagesKHR(m_VK_swapChain);
    m_VK_swapChainImageFormat = surfaceFormat.format;
    m_VK_swapChainExtent = extent;

}

void Craig::Renderer::createImageViews() {


    m_VK_swapChainImageViews.resize(m_VK_swapChainImages.size());

    for (size_t i = 0; i < m_VK_swapChainImages.size(); i++)
    {

        vk::ImageViewCreateInfo createInfo;
        createInfo.setImage(m_VK_swapChainImages[i])
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(m_VK_swapChainImageFormat)
            .setComponents(vk::ComponentMapping{
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity,
                vk::ComponentSwizzle::eIdentity
                })
            .setSubresourceRange(vk::ImageSubresourceRange{
                vk::ImageAspectFlagBits::eColor, // aspectMask
                0, 1, // baseMipLevel, levelCount
                0, 1  // baseArrayLayer, layerCount
                });


        try {
            m_VK_swapChainImageViews[i] = m_VK_device.createImageView(createInfo);
        }
        catch (const vk::SystemError& err) {
            throw std::runtime_error("failed to create image views!");
        }

    }
    
}

void Craig::Renderer::createGraphicsPipeline() {

    vertShaderModule = Craig::ShaderCompilation::CompileHLSLToShaderModule(m_VK_device, L"data/shaders/VertexShader.vert");
    fragShaderModule = Craig::ShaderCompilation::CompileHLSLToShaderModule(m_VK_device, L"data/shaders/FragmentShader.frag");

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo;

    vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(vertShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo;

    fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(fragShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

}