#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION

#include <cassert>
#include <iostream>
#include <set>
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>
#include "../External/stb_image.h"
#include "../External/tiny_obj_loader.h"


#if defined(IMGUI_ENABLED)
#include "../External/Imgui/imgui.h"   
#include "../External/Imgui/imgui_impl_vulkan.h"
#include "../External/Imgui/imgui_impl_sdl2.h"
#endif

#include "Craig_Renderer.hpp"
#include "Craig_Window.hpp"
#include "Craig_ShaderCompilation.hpp"

#if defined(IMGUI_ENABLED)
static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan-imgui] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}
#endif


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

CraigError Craig::Renderer::init(Window* CurrentWindowPtr) {

	CraigError ret = CRAIG_SUCCESS;

	// Check if the current window pointer is valid
	assert(CurrentWindowPtr != nullptr && "CurrentWindowPtr is null, cannot initialize Renderer without a valid window pointer.");

	//Pass in the current window pointer (Done in framework)
	mp_CurrentWindow = CurrentWindowPtr; 

	// Ensure that the current window pointer is not null (just to be extra safe)
	assert(mp_CurrentWindow != nullptr && "mp_CurrentWindow is null, somehow didn't get passed to our member variable");

	// Use validation layers if this is a debug build
#if defined(IMGUI_ENABLED)
	mv_VK_Layers.push_back("VK_LAYER_KHRONOS_validation");
    mp_CurrentWindow->getExtensionsVector().push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    // vk::ApplicationInfo allows the programmer to specifiy some basic information about the
    // program, which can be useful for layers and tools to provide more debug information.
    m_VK_appInfo = vk::ApplicationInfo()
        .setPApplicationName(kVK_AppName)
        .setApplicationVersion(kVK_AppVersion)
        .setPEngineName(kVK_EngineName)
        .setEngineVersion(kVK_EngineVersion)
        .setApiVersion(VK_API_VERSION_1_4);

    // vk::InstanceCreateInfo is where the programmer specifies the layers and/or extensions that
    // are needed.
    m_VK_instInfo = vk::InstanceCreateInfo()
        .setFlags(vk::InstanceCreateFlags())
        .setPApplicationInfo(&m_VK_appInfo)
        .setEnabledExtensionCount(static_cast<uint32_t>(mp_CurrentWindow->getExtensionsVector().size()))
        .setPpEnabledExtensionNames(mp_CurrentWindow->getExtensionsVector().data())
        .setEnabledLayerCount(static_cast<uint32_t>(mv_VK_Layers.size()))
        .setPpEnabledLayerNames(mv_VK_Layers.data());

	//Create the Vulkan instance/Initialize Vulkan
    try {
        VkValidationFeatureEnableEXT enabledFeatures[] = {
            VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
            // Optional extras:
            // VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
            // VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT
        };

        VkValidationFeaturesEXT featuresInfo{};
        featuresInfo.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        featuresInfo.enabledValidationFeatureCount = static_cast<uint32_t>(std::size(enabledFeatures));
        featuresInfo.pEnabledValidationFeatures = enabledFeatures;

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        populateDebugMessengerCreateInfo(debugCreateInfo);

        featuresInfo.pNext = &debugCreateInfo;
        m_VK_instInfo.setPNext(&featuresInfo);

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

#if defined(IMGUI_ENABLED)
    InitImgui();
#endif

	return ret;
}

#if defined(IMGUI_ENABLED)

void Craig::Renderer::InitImgui() {


    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForVulkan(mp_CurrentWindow->getSDLWindow());
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_VK_instance;
    init_info.PhysicalDevice = m_VK_physicalDevice;
    init_info.Device = m_VK_device;

    QueueFamilyIndices indices = findQueueFamilies(m_VK_physicalDevice);
    init_info.QueueFamily = indices.graphicsFamily.value();
    init_info.Queue = m_VK_graphicsQueue;
    init_info.DescriptorPool = m_VK_imguiDescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = kMaxFramesInFlight;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    init_info.RenderPass = m_VK_renderPass;
    ImGui_ImplVulkan_Init(&init_info);


}
#endif

CraigError Craig::Renderer::update() {

	CraigError ret = CRAIG_SUCCESS;

#if defined(IMGUI_ENABLED)
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_VK_physicalDevice);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
// Start the Dear ImGui frame
    if (extent.width > 0 || extent.height > 0) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode); //Lets transparency through the window
        ImGui::ShowDemoWindow(); // Show demo window! :)
    }
   
#endif

    drawFrame();



	return ret;
}


void Craig::Renderer::InitVulkan() {
    
    setupDebugMessenger(); // Actually enable the messenger
	pickPhysicalDevice(); // Pick a suitable physical device for rendering
	createLogicalDevice(); // Create a logical device to interact with the physical device

    initVMA();

    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createCommandPool();
    createDepthResources();
    createFrameBuffers();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    loadModel();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
#if defined(IMGUI_ENABLED)
    createImguiDescriptorPool();
#endif
    

}

void Craig::Renderer::initVMA() {

    vk::PhysicalDeviceProperties props = m_VK_physicalDevice.getProperties();

    VmaAllocatorCreateInfo vmaCreateInfo{};
    vmaCreateInfo.instance = m_VK_instance;
    vmaCreateInfo.physicalDevice = m_VK_physicalDevice;
    vmaCreateInfo.device = m_VK_device;
    vmaCreateInfo.vulkanApiVersion = VK_API_VERSION_1_4;
    //printf("Using vulkan api version: %i\n", props.apiVersion);

    VmaVulkanFunctions vmaFunctions{};                 // <-- important
    vmaFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vmaFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    vmaCreateInfo.pVulkanFunctions = &vmaFunctions;

    VkResult r = vmaCreateAllocator(&vmaCreateInfo, &m_VMA_allocator);
    if (r != VK_SUCCESS) throw std::runtime_error("vmaCreateAllocator failed");

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
    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilies) {
        //Skip if the queuecount is 0
        if (queueFamily.queueCount == 0) continue;

        //Skip if we already assigned the graphics family queue index
        if (!indices.graphicsFamily && (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)) {
            indices.graphicsFamily = i;
        }

        //Skip if we already assigned the presentation family queue index
        if(!indices.presentFamily && device.getSurfaceSupportKHR(i, m_VK_surface)) {
            indices.presentFamily = i; // If the queue family supports presentation to the surface, set the present family
		}


        if ((queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) && 
            !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) &&
            !(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
            !indices.transferFamily) {
            indices.transferFamily = i;
        }

        if (indices.isComplete() && indices.hasDedicatedTransfer()) {
			break; // If we already found a suitable family, no need to keep searching
        }

        i++;
    }

    // Fallback: if no dedicated transfer, use graphics (it’s implicitly transfer-capable)
    if (!indices.transferFamily && indices.graphicsFamily) {
        indices.transferFamily = indices.graphicsFamily;
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

    bool extensionsSupported = checkDeviceExtensionSupport(device); //Check it supports extensions, especifically the swapchain extension

    bool swapChainAdequate = false; 
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device); //Check the swapchaine extension it has is actually what we want for this
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    printf("Found graphics and presentation indices: %s\n", indices.isComplete() ? "True" : "False");
    printf("Found dedicated transfer index: %s\n", indices.hasDedicatedTransfer() ? "True" : "False");
    printf("Extensions (Like swapchain/double buffers) are supported: %s\n", extensionsSupported ? "True" : "False");
    printf("The swapchain extension is adequate for our use: %s\n", swapChainAdequate ? "True" : "False");

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

//Here we get the list of available device extensions, and check if the required ones are present
//by removing them from a set and checking if the set is empty at the end.
bool Craig::Renderer::checkDeviceExtensionSupport(const vk::PhysicalDevice& device) {

    std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();

	std::set<std::string> requiredExtensions(mv_VK_deviceExtensions.begin(), mv_VK_deviceExtensions.end());

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
    std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value() };
    

    float queuePriority = 1.0f; // Priority for the queue(s) we are creating (range: 0.0 to 1.0)

    // Create a vk::DeviceQueueCreateInfo for each unique queue family
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo = vk::DeviceQueueCreateInfo()
            .setQueueFamilyIndex(queueFamily)
            .setQueueCount(1)
            .setPQueuePriorities(&queuePriority);

        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::PhysicalDeviceFeatures deviceFeatures = m_VK_physicalDevice.getFeatures(); // Enable desired features (none yet, placeholder)
    deviceFeatures.setSamplerAnisotropy(vk::True);

    // Fill in device creation info with queue setup and feature requirements
    vk::DeviceCreateInfo createInfo = vk::DeviceCreateInfo()
        .setQueueCreateInfos(queueCreateInfos)
        .setPEnabledFeatures(&deviceFeatures)
        .setPEnabledExtensionNames(mv_VK_deviceExtensions);

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

    if (indices.hasDedicatedTransfer()) {
        m_VK_transferQueue = m_VK_device.getQueue(indices.transferFamily.value(), 0);
    }
    else {
        m_VK_transferQueue = m_VK_graphicsQueue;
    }
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

    mv_VK_swapChainImages = m_VK_device.getSwapchainImagesKHR(m_VK_swapChain);
    m_VK_swapChainImageFormat = surfaceFormat.format;
    m_VK_swapChainExtent = extent;

}

vk::ImageView Craig::Renderer::createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) {
    vk::ImageViewCreateInfo createInfo;
    createInfo.setImage(image)
        .setViewType(vk::ImageViewType::e2D)
        .setFormat(format)
        .setComponents(vk::ComponentMapping{
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity,
            vk::ComponentSwizzle::eIdentity
            })
        .setSubresourceRange(vk::ImageSubresourceRange{
            aspectFlags,// aspectMask
            0, 1, // baseMipLevel, levelCount
            0, 1  // baseArrayLayer, layerCount
            });


    vk::ImageView imageView;

    try {
        imageView = m_VK_device.createImageView(createInfo);
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to create image views!");
    }

    return imageView;
}

void Craig::Renderer::createImageViews() {


    mv_VK_swapChainImageViews.resize(mv_VK_swapChainImages.size());

    for (size_t i = 0; i < mv_VK_swapChainImages.size(); i++)
    {
        mv_VK_swapChainImageViews[i] = createImageView(mv_VK_swapChainImages[i], m_VK_swapChainImageFormat, vk::ImageAspectFlagBits::eColor);

    }
    
}

void Craig::Renderer::createGraphicsPipeline() {

    // Compile HLSL shaders to SPIR-V shader modules
    m_VK_vertShaderModule = Craig::ShaderCompilation::CompileHLSLToShaderModule(m_VK_device, L"data/shaders/VertexShader.vert");
    m_VK_fragShaderModule = Craig::ShaderCompilation::CompileHLSLToShaderModule(m_VK_device, L"data/shaders/FragmentShader.frag");

    // Set up shader stages for the pipeline
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
    vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(m_VK_vertShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
    fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(m_VK_fragShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    vk::VertexInputBindingDescription                   bindingDescription = Vertex::getBindingDescription();
    std::array<vk::VertexInputAttributeDescription, 3>  attributeDescriptions = Vertex::getAttributeDescriptions();

    //No vertex data to load for now since its hardcoded into the shader.
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.setVertexBindingDescriptionCount(1)
        .setPVertexBindingDescriptions(&bindingDescription) //These should point to an array of structs w vertex descriptions
        .setVertexAttributeDescriptionCount(static_cast<uint32_t>(attributeDescriptions.size()))
        .setPVertexAttributeDescriptions(attributeDescriptions.data());

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList)
        .setPrimitiveRestartEnable(vk::False);

    // Viewport/scissor are dynamic (set later in the command buffer)
    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.setViewportCount(1)
        .setScissorCount(1);

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.setDepthClampEnable(vk::False)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setLineWidth(1.0f)
        .setCullMode(vk::CullModeFlagBits::eBack)
        .setFrontFace(vk::FrontFace::eCounterClockwise)
        .setDepthBiasEnable(false);

    //Multisampling/Anti-Aliasing
    //Keeping it disabled for now but will follow up later in the tutorial
    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.setSampleShadingEnable(vk::False)
        .setRasterizationSamples(vk::SampleCountFlagBits::e1);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil
        .setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(false)
        .setStencilTestEnable(false);

    vk::PipelineColorBlendAttachmentState colourBlendAttachment;
    colourBlendAttachment.setColorWriteMask(
        vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA)
        .setBlendEnable(vk::False);

    vk::PipelineColorBlendStateCreateInfo colourBlending;
    colourBlending.setLogicOpEnable(vk::False)
        .setLogicOp(vk::LogicOp::eCopy)
        .setAttachmentCount(1)
        .setPAttachments(&colourBlendAttachment);

    //Some bits of the pipeline can be changed, like the viewport, without having to recreate the pipeline/bake them again
    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.setDynamicStateCount(static_cast<uint32_t>(dynamicStates.size()))
        .setPDynamicStates(dynamicStates.data());


    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setSetLayoutCount(1)
        .setSetLayouts(m_VK_descriptorSetLayout);

    try {
        m_VK_pipelineLayout = m_VK_device.createPipelineLayout(pipelineLayoutInfo);
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to createPipelineLayout!");
    }

    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo
        .setStageCount(2)
        .setPStages(shaderStages)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPDepthStencilState(&depthStencil)
        .setPColorBlendState(&colourBlending)
        .setPDynamicState(&dynamicState)
        .setLayout(m_VK_pipelineLayout)
        .setRenderPass(m_VK_renderPass)
        .setSubpass(0);


    auto result = m_VK_device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo);

    if (result.result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
    m_VK_graphicsPipeline = result.value;


}

void Craig::Renderer::createRenderPass() {

    vk::AttachmentDescription colourAttachment;
    colourAttachment
        .setFormat(m_VK_swapChainImageFormat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentDescription depthAttatchment;
    depthAttatchment
        .setFormat(findDepthFormat())
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentReference colourAttachmentRef;
    colourAttachmentRef
        .setAttachment(0) //We only have one attachment description so it'll go to that
        .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentReference depthAttachmentRef;
    depthAttachmentRef
        .setAttachment(1)
        .setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::SubpassDescription subpass;
    subpass
        .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&colourAttachmentRef)
        .setPDepthStencilAttachment(&depthAttachmentRef);

    vk::SubpassDependency dependency;
    dependency
        .setSrcSubpass(vk::SubpassExternal)
        .setDstSubpass(0)
        .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eLateFragmentTests)
        .setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite)
        .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests)
        .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);


    std::array<vk::AttachmentDescription, 2> attachments = { colourAttachment, depthAttatchment };

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo
        .setAttachmentCount(static_cast<uint32_t>(attachments.size()))
        .setPAttachments(attachments.data())
        .setSubpassCount(1)
        .setPSubpasses(&subpass)
        .setDependencyCount(1)
        .setDependencies(dependency);




    try {
        m_VK_renderPass = m_VK_device.createRenderPass(renderPassInfo);
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to create render pass!");
    }

}

void Craig::Renderer::createFrameBuffers() {

    mv_VK_swapChainFramebuffers.resize(mv_VK_swapChainImageViews.size());

    for (size_t i = 0; i < mv_VK_swapChainImageViews.size(); i++)
    {
        std::array<vk::ImageView, 2> attachments = {
            mv_VK_swapChainImageViews[i],
            m_VK_depthImageView
        };

        vk::FramebufferCreateInfo frameBufferInfo;
        frameBufferInfo
            .setRenderPass(m_VK_renderPass)
            .setAttachmentCount(static_cast<uint32_t>(attachments.size()))
            .setPAttachments(attachments.data())
            .setWidth(m_VK_swapChainExtent.width)
            .setHeight(m_VK_swapChainExtent.height)
            .setLayers(1);

        try {
            mv_VK_swapChainFramebuffers[i] = m_VK_device.createFramebuffer(frameBufferInfo);
        }
        catch (const vk::SystemError& err) {
            throw std::runtime_error("failed to create framebuffer!");
        }
        


    };

}

void Craig::Renderer::cleanupSwapChain() {

    m_VK_device.destroyImageView(m_VK_depthImageView);
    vmaDestroyImage(m_VMA_allocator, m_VK_depthImage, m_VMA_depthImageAllocation);

    for (auto framebuffer : mv_VK_swapChainFramebuffers) {
        m_VK_device.destroyFramebuffer(framebuffer);
    }

    for (auto imageView : mv_VK_swapChainImageViews) {
        m_VK_device.destroyImageView(imageView);
    }

    m_VK_device.destroySwapchainKHR(m_VK_swapChain);
}

void Craig::Renderer::recreateSwapChain() {

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_VK_physicalDevice);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    if (extent.width <= 0 || extent.height <= 0) {
        return; // Skip this frame
    }


    m_VK_device.waitIdle();

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFrameBuffers();
}

void Craig::Renderer::createCommandPool() {

    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_VK_physicalDevice);

    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
        .setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value());

    m_VK_commandPool = m_VK_device.createCommandPool(poolInfo);

    if (queueFamilyIndices.hasDedicatedTransfer()) {
        vk::CommandPoolCreateInfo info;
        info.setQueueFamilyIndex(queueFamilyIndices.transferFamily.value())
            .setFlags(vk::CommandPoolCreateFlagBits::eTransient); // copies are short-lived

        m_VK_transferCommandPool = m_VK_device.createCommandPool(info);
    }
    else {
        // No dedicated transfer family — reuse graphics pool
        m_VK_transferCommandPool = m_VK_commandPool;
    }

}

void Craig::Renderer::createCommandBuffers() {
    mv_VK_commandBuffers.resize(kMaxFramesInFlight);

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setCommandPool(m_VK_commandPool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount((uint32_t)mv_VK_commandBuffers.size());

    try {
        mv_VK_commandBuffers = m_VK_device.allocateCommandBuffers(allocInfo);
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to allocate command buffers!");
    }


}

void Craig::Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {

    vk::CommandBufferBeginInfo beginInfo;

    if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // Set up render pass for drawing to the framebuffer
    vk::RenderPassBeginInfo renderPassInfo;
    renderPassInfo.setRenderPass(m_VK_renderPass)
        .setFramebuffer(mv_VK_swapChainFramebuffers[imageIndex])
        .renderArea.setOffset({ 0, 0 })
                   .setExtent(m_VK_swapChainExtent);

    // Clear color for this frame
    std::array<vk::ClearValue, 2> clearValues;
    clearValues[0].setColor({kClearColour[0], kClearColour[1], kClearColour[2], kClearColour[3]});
    clearValues[1].setDepthStencil({ 1.0f, 0 });

    renderPassInfo.setClearValueCount(static_cast<uint32_t>(clearValues.size()))
        .setPClearValues(clearValues.data());

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    
    //Binding the vertex buffer
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_VK_graphicsPipeline);
    vk::Buffer vertexBuffers[] = { m_VK_vertexBuffer };
    vk::DeviceSize offsets[] = { 0 };
    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(m_VK_indexBuffer, 0, vk::IndexType::eUint32);

    // Set the dynamic viewport (covers the whole framebuffer)
    vk::Viewport viewport;
    viewport.setX(0.0f)
        .setY(0.0f)
        .setWidth(static_cast<float>(m_VK_swapChainExtent.width))
        .setHeight(static_cast<float>(m_VK_swapChainExtent.height))
        .setMinDepth(0.0f)
        .setMaxDepth(1.0f);

    commandBuffer.setViewport(0, viewport);

    // Set the dynamic scissor (no cropping — covers entire area)
    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 })
        .setExtent(m_VK_swapChainExtent);

    commandBuffer.setScissor(0, scissor);

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_VK_pipelineLayout, 0, m_VK_descriptorSets[m_currentFrame], nullptr);

    /*
    indexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
    instanceCount: Used for instanced rendering, use 1 if you're not doing that.
    firstIndex: Used as an offset into the index buffer
    vertexOffset: used as an offset into the vertex buffer?
    firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
    */
    commandBuffer.drawIndexed(static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);

#if defined(IMGUI_ENABLED)
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
#endif

    commandBuffer.endRenderPass();


    try {
        commandBuffer.end();
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to record command buffer!");
    }

    
}

void Craig::Renderer::createSyncObjects() {

    vk::SemaphoreCreateInfo semaphoreInfo;
    vk::FenceCreateInfo fenceInfo;
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);  // Start signaled so the first frame doesn't block

    mv_VK_renderFinishedSemaphores.resize(kMaxFramesInFlight);
    mv_VK_imageAvailableSemaphores.resize(kMaxFramesInFlight);
    mv_VK_inFlightFences.resize(kMaxFramesInFlight);

    try {
        
        
        for (size_t i = 0; i < kMaxFramesInFlight; i++) {
            mv_VK_imageAvailableSemaphores[i] = m_VK_device.createSemaphore(semaphoreInfo);
            mv_VK_renderFinishedSemaphores[i] = m_VK_device.createSemaphore(semaphoreInfo);
            mv_VK_inFlightFences[i] = m_VK_device.createFence(fenceInfo);
        }

       
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to record command buffer!");
    }

}

void Craig::Renderer::createBufferVMA(
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    const VmaAllocationCreateInfo& aci,
    vk::Buffer& buffer,
    VmaAllocation& alloc,
    VmaAllocationInfo* outInfo)
{
    VkBufferCreateInfo bi{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bi.size = size;
    bi.usage = static_cast<VkBufferUsageFlags>(usage);
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // If you truly need concurrent:
    if (auto idx = findQueueFamilies(m_VK_physicalDevice); idx.hasDedicatedTransfer()) {
        uint32_t q[2] = { idx.graphicsFamily.value(), idx.transferFamily.value() };
        bi.sharingMode = VK_SHARING_MODE_CONCURRENT;
        bi.queueFamilyIndexCount = 2;
        bi.pQueueFamilyIndices = q;
    }

    VkBuffer raw{};
    vmaCreateBuffer(m_VMA_allocator, &bi, &aci, &raw, &alloc, outInfo);
    buffer = vk::Buffer(raw);
}

vk::CommandBuffer Craig::Renderer::buffer_beginSingleTimeCommands() {
    //Allocate a temporary command buffer
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(m_VK_transferCommandPool)
        .setCommandBufferCount(1);


    vk::CommandBuffer commandBuffer = m_VK_device.allocateCommandBuffers(allocInfo)[0];

    //Start recording the command buffer
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Craig::Renderer::buffer_endSingleTimeCommands(vk::CommandBuffer commandBuffer) {

    //Stop recording
    commandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.setCommandBufferCount(1)
        .setCommandBuffers(commandBuffer);

    m_VK_transferQueue.submit(submitInfo);
    m_VK_transferQueue.waitIdle(); //We could use a fence instead to allow multiple transfers simultaniously.

    m_VK_device.freeCommandBuffers(m_VK_transferCommandPool, commandBuffer);
}

vk::CommandBuffer Craig::Renderer::buffer_beginSingleTimeCommandsGFX() {
    //Allocate a temporary command buffer
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(m_VK_commandPool)
        .setCommandBufferCount(1);


    vk::CommandBuffer commandBuffer = m_VK_device.allocateCommandBuffers(allocInfo)[0];

    //Start recording the command buffer
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Craig::Renderer::buffer_endSingleTimeCommandsGFX(vk::CommandBuffer commandBuffer) {

    //Stop recording
    commandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.setCommandBufferCount(1)
        .setCommandBuffers(commandBuffer);

    m_VK_graphicsQueue.submit(submitInfo);
    m_VK_graphicsQueue.waitIdle(); //We could use a fence instead to allow multiple transfers simultaniously.

    m_VK_device.freeCommandBuffers(m_VK_commandPool, commandBuffer);
}

void Craig::Renderer::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {

    //Begin recording to buffer
    vk::CommandBuffer tempBuffer = buffer_beginSingleTimeCommands();

    //Copy over the data
    vk::BufferCopy copyRegion;
    copyRegion.setSize(size);
    tempBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);

    //End recording and submit buffer
    buffer_endSingleTimeCommands(tempBuffer);


}


//For us to copy the buffer that contains the picture data to the vulkan image, we need to make sure it's the right layout first
void Craig::Renderer::transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, bool useTransferQueue) {
    //Begin recording to buffer
    vk::CommandBuffer tempBuffer = useTransferQueue ? buffer_beginSingleTimeCommands() : buffer_beginSingleTimeCommandsGFX();

    vk::ImageMemoryBarrier barrier;
    barrier.setOldLayout(oldLayout)
        .setNewLayout(newLayout)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
        /*From vulkan-tutorial.com 
        The image and subresourceRange specify the image that is affected and the specific part of the image.
        Our image is not an array and does not have mipmapping levels, so only one level and layer are specified.*/
        .setImage(image)
        .subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
                         .setBaseMipLevel(0)
                         .setLevelCount(1)
                         .setBaseArrayLayer(0)
                         .setLayerCount(1);

    vk::PipelineStageFlags sourceStage, destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        //TODO
        barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if(oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal){

        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);

        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;

        
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    tempBuffer.pipelineBarrier(sourceStage, destinationStage,
        vk::DependencyFlagBits::eByRegion, 
        nullptr, 
        nullptr, 
        barrier);


    useTransferQueue ? buffer_endSingleTimeCommands(tempBuffer) : buffer_endSingleTimeCommandsGFX(tempBuffer);

}

void Craig::Renderer::copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) {
    vk::CommandBuffer tempBuffer = buffer_beginSingleTimeCommands();

    vk::BufferImageCopy region;
    region.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0);

    region.imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setMipLevel(0)
        .setBaseArrayLayer(0)
        .setLayerCount(1);

    region.setImageOffset({ 0, 0, 0 })
        .setImageExtent({
            width,
            height,
            1
            });

    tempBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);


    buffer_endSingleTimeCommands(tempBuffer);
}

void Craig::Renderer::createVertexBuffer() {
    vk::DeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();

    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc{};

    VmaAllocationCreateInfo stagingAci{};
    stagingAci.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, stagingAci, stagingBuffer, stagingAlloc);

    void* data;
    vmaMapMemory(m_VMA_allocator, stagingAlloc, &data);
    memcpy(data, m_vertices.data(), (size_t)bufferSize);
    vmaFlushAllocation(m_VMA_allocator, stagingAlloc, 0, bufferSize);
    vmaUnmapMemory(m_VMA_allocator, stagingAlloc);

    VmaAllocationCreateInfo gpuAci{};
    gpuAci.usage = VMA_MEMORY_USAGE_AUTO;
    gpuAci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, gpuAci, m_VK_vertexBuffer, m_VMA_vertexAllocation);

    copyBuffer(stagingBuffer, m_VK_vertexBuffer, bufferSize);
    vmaDestroyBuffer(m_VMA_allocator, stagingBuffer, stagingAlloc);
}

void Craig::Renderer::createIndexBuffer() {
    vk::DeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();

    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc{};

    VmaAllocationCreateInfo stagingAci{};
    stagingAci.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, stagingAci, stagingBuffer, stagingAlloc);

    void* data;
    vmaMapMemory(m_VMA_allocator, stagingAlloc, &data);
    memcpy(data, m_indices.data(), (size_t)bufferSize);
    vmaFlushAllocation(m_VMA_allocator, stagingAlloc, 0, bufferSize);
    vmaUnmapMemory(m_VMA_allocator, stagingAlloc);

    VmaAllocationCreateInfo gpuAci{};
    gpuAci.usage = VMA_MEMORY_USAGE_AUTO;
    gpuAci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, gpuAci, m_VK_indexBuffer, m_VMA_indexAllocation);

    copyBuffer(stagingBuffer, m_VK_indexBuffer, bufferSize);
    vmaDestroyBuffer(m_VMA_allocator, stagingBuffer, stagingAlloc);
}



//From vulkan-tutorial.com
//The descriptor set layout specifies the types of resources that are going to be accessed by the pipeline, just like a render pass specifies the types of attachments that will be accessed. 
//
//A descriptor set specifies the actual buffer or image resources that will be bound to the descriptors, just like a framebuffer specifies the actual image views to bind to render pass attachments.
void Craig::Renderer::createDescriptorSetLayout() {

    vk::DescriptorSetLayoutBinding uboLayoutBinding;
    uboLayoutBinding
        .setBinding(0)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex);

    vk::DescriptorSetLayoutBinding samplerLayourBinding;
    samplerLayourBinding
        .setBinding(1)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setPImmutableSamplers(nullptr)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment);

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayourBinding };

    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo
        .setBindingCount(static_cast<uint32_t>(bindings.size()))
        .setBindings(bindings);

    m_VK_descriptorSetLayout = m_VK_device.createDescriptorSetLayout(layoutInfo);


}


void Craig::Renderer::createUniformBuffers() {

    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
    
    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc{};

    VmaAllocationCreateInfo stagingAci{};
    stagingAci.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    stagingAci.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    mv_VK_uniformBuffers.resize(kMaxFramesInFlight);
    mv_VK_uniformBuffersAllocations.resize(kMaxFramesInFlight);
    mv_VK_uniformBuffersMapped.resize(kMaxFramesInFlight);

    for (size_t i = 0; i < kMaxFramesInFlight; i++)
    {
        VmaAllocationInfo info{};
        createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, stagingAci, mv_VK_uniformBuffers[i], mv_VK_uniformBuffersAllocations[i], &info);

        mv_VK_uniformBuffersMapped[i] = info.pMappedData;

    }


}

void Craig::Renderer::createDescriptorPool() {

    std::array<vk::DescriptorPoolSize, 2> poolSizes;
    poolSizes[0]
        .setType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(static_cast<uint32_t>(kMaxFramesInFlight));
    poolSizes[1]
        .setType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(static_cast<uint32_t>(kMaxFramesInFlight));

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo
        .setPoolSizeCount(static_cast<uint32_t>(poolSizes.size()))
        .setPoolSizes(poolSizes)
        .setMaxSets(static_cast<uint32_t>(kMaxFramesInFlight));
   
    m_VK_descriptorPool = m_VK_device.createDescriptorPool(poolInfo);

}

void Craig::Renderer::createDescriptorSets() {

    std::vector<vk::DescriptorSetLayout> layouts(kMaxFramesInFlight, m_VK_descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.setDescriptorPool(m_VK_descriptorPool)
        .setDescriptorSetCount(static_cast<uint32_t>(kMaxFramesInFlight))
        .setSetLayouts(layouts);

    m_VK_descriptorSets.resize(kMaxFramesInFlight);
    m_VK_descriptorSets = m_VK_device.allocateDescriptorSets(allocInfo);

    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.setBuffer(mv_VK_uniformBuffers[i])
            .setOffset(0)
            .setRange(sizeof(UniformBufferObject));

        vk::DescriptorImageInfo imageInfo;
        imageInfo
            .setImageView(m_VK_textureImageView)
            .setSampler(m_VK_textureSampler)
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0]
            .setDstSet(m_VK_descriptorSets[i])
            .setDstBinding(0)
            .setDstArrayElement(0)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1)
            .setBufferInfo(bufferInfo);

        descriptorWrites[1]
            .setDstSet(m_VK_descriptorSets[i])
            .setDstBinding(1)
            .setDstArrayElement(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(1)
            .setImageInfo(imageInfo);

        m_VK_device.updateDescriptorSets(descriptorWrites, nullptr);

    }

}

//TODO: Separate camera
void Craig::Renderer::updateUniformBuffer(uint32_t currentImage) {

    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo;

    //lets rotate the model around the z axis dependent on time
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    //Look at geomtry from above at 45 degree angle
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    //45 degree FOV perspective projection.
    ubo.proj = glm::perspective(glm::radians(45.0f), m_VK_swapChainExtent.width / (float)m_VK_swapChainExtent.height, 0.1f, 10.0f);

    //from vulkan-tutorial
    /*GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted. 
    The easiest way to compensate for that is to flip the sign on the scaling factor of the Y axis in the projection matrix. 
    If you don't do this, then the image will be rendered upside down.*/
    ubo.proj[1][1] *= -1;

    memcpy(mv_VK_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void Craig::Renderer::createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc{};

    VmaAllocationCreateInfo stagingAci{};
    stagingAci.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    createBufferVMA(imageSize, vk::BufferUsageFlagBits::eTransferSrc, stagingAci, stagingBuffer, stagingAlloc);

    void* data;
    vmaMapMemory(m_VMA_allocator, stagingAlloc, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vmaFlushAllocation(m_VMA_allocator, stagingAlloc, 0, imageSize);
    vmaUnmapMemory(m_VMA_allocator, stagingAlloc);

    stbi_image_free(pixels);

    createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, m_VK_textureImage, m_VMA_textureImageAllocation);

    transitionImageLayout(m_VK_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copyBufferToImage(stagingBuffer, m_VK_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    transitionImageLayout(m_VK_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, false);

    vmaDestroyBuffer(m_VMA_allocator, stagingBuffer, stagingAlloc);
}

void Craig::Renderer::createTextureImageView() {
    
    m_VK_textureImageView = createImageView(m_VK_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

void Craig::Renderer::createTextureSampler() {
    
    vk::PhysicalDeviceProperties physicalDeviceProperties;
    physicalDeviceProperties = m_VK_physicalDevice.getProperties();

    vk::SamplerCreateInfo samplerInfo;

    samplerInfo
        //How to interpolate texels that are magnified or minified
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        //What to do when we try to read texels outside the image
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        //Enable anisotropic filtering
        .setAnisotropyEnable(vk::True)
        .setMaxAnisotropy(physicalDeviceProperties.limits.maxSamplerAnisotropy) //We can set it to whatever the max the gpu supports
        //Specify colour returned when sampling beyond the image with clamp to border mode
        .setBorderColor(vk::BorderColor::eFloatOpaqueBlack)
        //Clamp coordinates to 0-1 or 0-texSize
        .setUnnormalizedCoordinates(vk::False)
        //Something about comparing the texels
        .setCompareEnable(vk::False)
        .setCompareOp(vk::CompareOp::eAlways)
        //Mimapping
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setMipLodBias(0.0f)
        .setMinLod(0.0f)
        .setMaxLod(0.0f);
    
    m_VK_textureSampler = m_VK_device.createSampler(samplerInfo);



}

void Craig::Renderer::createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, VmaAllocation& allocation) {

    vk::ImageCreateInfo imageInfo;
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.extent.setWidth(width);
    imageInfo.extent.setHeight(height);
    imageInfo.extent.setDepth(1);
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(1);
    imageInfo.setFormat(format);
    imageInfo.setTiling(tiling); /*
        VK_IMAGE_TILING_LINEAR: Texels are laid out in row - major order like our pixels array
        VK_IMAGE_TILING_OPTIMAL : Texels are laid out in an implementation defined order for optimal access */
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setUsage(usage);
    imageInfo.setSamples(vk::SampleCountFlagBits::e1); //From Vulkan-Tutorial.com - The samples flag is related to multisampling. This is only relevant for images that will be used as attachments, so stick to one sample.

    QueueFamilyIndices q = findQueueFamilies(m_VK_physicalDevice);
    if (q.hasDedicatedTransfer()) {
        uint32_t families[] = { q.graphicsFamily.value(), q.transferFamily.value() };

        imageInfo.setSharingMode(vk::SharingMode::eConcurrent);
        imageInfo.setQueueFamilyIndexCount(2);
        imageInfo.setQueueFamilyIndices(families);
    }
    else {
        imageInfo.setSharingMode(vk::SharingMode::eExclusive);
    }

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;

    if (properties & vk::MemoryPropertyFlagBits::eDeviceLocal)
        aci.requiredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (properties & vk::MemoryPropertyFlagBits::eHostVisible)
        aci.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    VkResult result = vmaCreateImage(m_VMA_allocator, imageInfo, &aci, reinterpret_cast<VkImage*>(&image), &allocation, nullptr);

    if (result != VK_SUCCESS)
        throw std::runtime_error("vmaCreateImage failed");

  

}

vk::Format Craig::Renderer::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
    for (auto format : candidates) {
        auto props = m_VK_physicalDevice.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features)
            return format;

        if (tiling == vk::ImageTiling::eOptimal &&
            (props.optimalTilingFeatures & features) == features)
            return format;
    }

    throw std::runtime_error("no supported depth format found");
}

vk::Format Craig::Renderer::findDepthFormat() {
    return findSupportedFormat(
        {
            vk::Format::eD32SfloatS8Uint,
            vk::Format::eD24UnormS8Uint,
            vk::Format::eD32Sfloat
        },
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

bool Craig::Renderer::hasStencilComponent(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

void Craig::Renderer::createDepthResources() {

    vk::Format depthFormat = findDepthFormat();

    createImage(m_VK_swapChainExtent.width, m_VK_swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, m_VK_depthImage, m_VMA_depthImageAllocation);

    m_VK_depthImageView = createImageView(m_VK_depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);

}

void Craig::Renderer::drawFrame() {

    // Wait until the previous frame has finished
    m_VK_device.waitForFences(mv_VK_inFlightFences[m_currentFrame], vk::True, UINT64_MAX);

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_VK_physicalDevice);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    if (extent.width <= 0 || extent.height <= 0) {
        return; // Skip this frame
    }

    uint32_t imageIndex;
    VkResult nextImageResult = vkAcquireNextImageKHR(m_VK_device, m_VK_swapChain, UINT64_MAX, mv_VK_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (nextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    else if (nextImageResult != VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }


    m_VK_device.resetFences(mv_VK_inFlightFences[m_currentFrame]);

    

    // Record drawing commands into the command buffer
    mv_VK_commandBuffers[m_currentFrame].reset();
    recordCommandBuffer(mv_VK_commandBuffers[m_currentFrame], imageIndex);

    updateUniformBuffer(m_currentFrame);

    // Submit the command buffer for execution
    vk::SubmitInfo submitInfo;

    vk::Semaphore waitSemaphores[] = { mv_VK_imageAvailableSemaphores[m_currentFrame]};
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    submitInfo.setWaitSemaphoreCount(1)
        .setPWaitSemaphores(waitSemaphores)
        .setPWaitDstStageMask(waitStages)
        .setCommandBufferCount(1)
        .setPCommandBuffers(&mv_VK_commandBuffers[m_currentFrame]);

    vk::Semaphore signalSemaphores[] = { mv_VK_renderFinishedSemaphores[m_currentFrame] };
    submitInfo.setSignalSemaphoreCount(1)
        .setPSignalSemaphores(signalSemaphores);

    try {
        m_VK_graphicsQueue.submit(submitInfo, mv_VK_inFlightFences[m_currentFrame]);
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }
    
    // Present the rendered image to the screen
    vk::PresentInfoKHR presentInfo;
    presentInfo.setWaitSemaphoreCount(1)
        .setPWaitSemaphores(signalSemaphores);

    vk::SwapchainKHR swapChains[] = { m_VK_swapChain };
    presentInfo.setSwapchainCount(1)
        .setPSwapchains(swapChains)
        .setPImageIndices(&imageIndex);


    //We have to revert back to the original C code otherwise if it returns ERROR_OUT_OF_DATE, it throws an exception and messes up the code.
    auto presentResult = vkQueuePresentKHR(m_VK_presentationQueue, presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || mp_CurrentWindow->isResizeNeeded()) {
        recreateSwapChain();
        mp_CurrentWindow->finishedResize();
    }
    else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;

}


#if defined(IMGUI_ENABLED)
void Craig::Renderer::createImguiDescriptorPool() {

    vk::DescriptorPoolSize poolSize = {
    vk::DescriptorType::eCombinedImageSampler,
    IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE
    };

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setMaxSets(poolSize.descriptorCount)
        .setPoolSizeCount(1)
        .setPoolSizes(poolSize);

    vk::Result result = m_VK_device.createDescriptorPool(&poolInfo, nullptr, &m_VK_imguiDescriptorPool);
    check_vk_result(static_cast<VkResult>(result));
}
#endif

CraigError Craig::Renderer::terminate() {

    CraigError ret = CRAIG_SUCCESS;

    m_VK_device.waitIdle();

#if defined(IMGUI_ENABLED)
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_VK_device.destroyDescriptorPool(m_VK_imguiDescriptorPool);
#endif
    vmaDestroyBuffer(m_VMA_allocator, m_VK_indexBuffer, m_VMA_indexAllocation);

    vmaDestroyBuffer(m_VMA_allocator, m_VK_vertexBuffer, m_VMA_vertexAllocation);

    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        m_VK_device.destroySemaphore(mv_VK_imageAvailableSemaphores[i]);
        m_VK_device.destroySemaphore(mv_VK_renderFinishedSemaphores[i]);
        m_VK_device.destroyFence(mv_VK_inFlightFences[i]);
    }    

    if (m_VK_transferCommandPool && m_VK_transferCommandPool != m_VK_commandPool) {
        m_VK_device.destroyCommandPool(m_VK_transferCommandPool);
    }
       
    m_VK_device.destroyCommandPool(m_VK_commandPool);

    for (auto framebuffer : mv_VK_swapChainFramebuffers) {
        m_VK_device.destroyFramebuffer(framebuffer);
    }

    m_VK_device.destroyImageView(m_VK_depthImageView);
    vmaDestroyImage(m_VMA_allocator, m_VK_depthImage, m_VMA_depthImageAllocation);

    m_VK_device.destroySampler(m_VK_textureSampler);

    m_VK_device.destroyImageView(m_VK_textureImageView);

    vmaDestroyImage(m_VMA_allocator, m_VK_textureImage, m_VMA_textureImageAllocation);

    m_VK_device.destroyPipeline(m_VK_graphicsPipeline);
    m_VK_device.destroyPipelineLayout(m_VK_pipelineLayout);
    m_VK_device.destroyRenderPass(m_VK_renderPass);

    m_VK_device.destroyShaderModule(m_VK_vertShaderModule);
    m_VK_device.destroyShaderModule(m_VK_fragShaderModule);

    for (auto imageView : mv_VK_swapChainImageViews) {
        m_VK_device.destroyImageView(imageView);
    }


    m_VK_device.destroySwapchainKHR(m_VK_swapChain);

    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        vmaDestroyBuffer(m_VMA_allocator, mv_VK_uniformBuffers[i], mv_VK_uniformBuffersAllocations[i]);
    }

    m_VK_device.destroyDescriptorPool(m_VK_descriptorPool);
    m_VK_device.destroyDescriptorSetLayout(m_VK_descriptorSetLayout);

    vmaDestroyAllocator(m_VMA_allocator);

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

vk::VertexInputBindingDescription Craig::Renderer::Vertex::getBindingDescription()  {
    vk::VertexInputBindingDescription bindingDescription;

    bindingDescription
        .setBinding(0)
        .setStride(sizeof(Vertex))
        .setInputRate(vk::VertexInputRate::eVertex);
       

    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, 3> Craig::Renderer::Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;

    attributeDescriptions[0]
        .setBinding(0) 
        .setLocation(0)//Location0 = POSITION0
        .setFormat(vk::Format::eR32G32B32Sfloat) //Not a colour, just uses the same format. Float2 = RG_float (Only 2 channels) 
        .setOffset(offsetof(Vertex, m_pos));

    attributeDescriptions[1]
        .setBinding(0) 
        .setLocation(1)//Location1 = COLOR1 <- ps fuck american spelling.
        .setFormat(vk::Format::eR32G32B32Sfloat) //This time it IS a colour, so float3 = RGB_float
        .setOffset(offsetof(Vertex, m_color));

    attributeDescriptions[2]
        .setBinding(0)
        .setLocation(2)
        .setFormat(vk::Format::eR32G32Sfloat) 
        .setOffset(offsetof(Vertex, m_texCoord));


    return attributeDescriptions;
}

uint32_t Craig::Renderer::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties;

    memProperties = m_VK_physicalDevice.getMemoryProperties();

    for (size_t i = 0; i < memProperties.memoryTypeCount; i++)
    {

        //VkMemoryRequirements::memoryTypeBits is a bitfield that sets a bit for every memoryType that is
        //supported for the resource.Therefore we need to check if the bit at index i is set while also testing the
        //required memory property flags while iterating over the memory types.

        //i kinda get it (i lie)
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");

}

void Craig::Renderer::loadModel() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    std::string warn;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str());

    if (!warn.empty()) {
        std::cout << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << err << std::endl;
    }

    if (!ret) {
        exit(CRAIG_FAIL);
    }


    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};



            vertex.m_pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.m_texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.m_color = { 1.0f, 1.0f, 1.0f };

            //TODO: unique vertex loading

            m_vertices.push_back(vertex);
            m_indices.push_back(m_indices.size());
        }
    }

}