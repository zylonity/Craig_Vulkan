#include <cassert>
#include <iostream>
#include <set>
#include <algorithm>

#if defined(_DEBUG)
#include "../External/Imgui/imgui.h"   
#include "../External/Imgui/imgui_impl_vulkan.h"
#include "../External/Imgui/imgui_impl_sdl2.h"
#endif

#include "Craig_Renderer.hpp"
#include "Craig_Window.hpp"
#include "Craig_ShaderCompilation.hpp"

#if defined(_DEBUG)
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

#if defined(_DEBUG)
    InitImgui();
#endif

	return ret;
}

#if defined(_DEBUG)

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

#if defined(_DEBUG)
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
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFrameBuffers();
    createCommandPool();
    createVertexBuffer();
    createIndexBuffer();
    createCommandBuffers();
    createSyncObjects();
#if defined(_DEBUG)
    createImguiDescriptorPool();
#endif
    

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
    std::array<vk::VertexInputAttributeDescription, 2>  attributeDescriptions = Vertex::getAttributeDescriptions();

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
        .setFrontFace(vk::FrontFace::eClockwise)
        .setDepthBiasEnable(false);

    //Multisampling/Anti-Aliasing
    //Keeping it disabled for now but will follow up later in the tutorial
    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.setSampleShadingEnable(vk::False)
        .setRasterizationSamples(vk::SampleCountFlagBits::e1);

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

    try {
        m_VK_pipelineLayout = m_VK_device.createPipelineLayout(pipelineLayoutInfo);
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to createPipelineLayout!");
    }

    vk::GraphicsPipelineCreateInfo pipelineInfo;

    pipelineInfo.setStageCount(2)
        .setPStages(shaderStages)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPDepthStencilState(nullptr)
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
    colourAttachment.setFormat(m_VK_swapChainImageFormat)
        .setSamples(vk::SampleCountFlagBits::e1)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
        .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setInitialLayout(vk::ImageLayout::eUndefined)
        .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference colourAttachmentRef;
    colourAttachmentRef.setAttachment(0) //We only have one attachment description so it'll go to that
        .setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass;
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&colourAttachmentRef);

    vk::SubpassDependency dependency;
    dependency.setSrcSubpass(vk::SubpassExternal)
        .setDstSubpass(0)
        .setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        .setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
        .setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.setAttachmentCount(1)
        .setPAttachments(&colourAttachment)
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

    m_VK_swapChainFramebuffers.resize(m_VK_swapChainImageViews.size());

    for (size_t i = 0; i < m_VK_swapChainImageViews.size(); i++)
    {
        vk::ImageView attachments[]{
            m_VK_swapChainImageViews[i]
        };

        vk::FramebufferCreateInfo frameBufferInfo;
        frameBufferInfo.setRenderPass(m_VK_renderPass)
            .setAttachmentCount(1)
            .setPAttachments(attachments)
            .setWidth(m_VK_swapChainExtent.width)
            .setHeight(m_VK_swapChainExtent.height)
            .setLayers(1);

        try {
            m_VK_swapChainFramebuffers[i] = m_VK_device.createFramebuffer(frameBufferInfo);
        }
        catch (const vk::SystemError& err) {
            throw std::runtime_error("failed to create framebuffer!");
        }
        


    };

}

void Craig::Renderer::cleanupSwapChain() {
    for (auto framebuffer : m_VK_swapChainFramebuffers) {
        m_VK_device.destroyFramebuffer(framebuffer);
    }

    for (auto imageView : m_VK_swapChainImageViews) {
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
    m_VK_commandBuffers.resize(kMaxFramesInFlight);

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setCommandPool(m_VK_commandPool)
        .setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandBufferCount((uint32_t)m_VK_commandBuffers.size());

    try {
        m_VK_commandBuffers = m_VK_device.allocateCommandBuffers(allocInfo);
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
        .setFramebuffer(m_VK_swapChainFramebuffers[imageIndex])
        .renderArea.setOffset({ 0, 0 })
                   .setExtent(m_VK_swapChainExtent);

    // Clear color for this frame
    vk::ClearValue clearValue;
    clearValue.setColor({ kClearColour[0], kClearColour[1], kClearColour[2], kClearColour[3] });
    renderPassInfo.setClearValueCount(1)
        .setPClearValues(&clearValue);

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    
    //Binding the vertex buffer
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_VK_graphicsPipeline);
    vk::Buffer vertexBuffers[] = { m_VK_vertexBuffer };
    vk::DeviceSize offsets[] = { 0 };
    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(m_VK_indexBuffer, 0, vk::IndexType::eUint16);

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



    /*
    indexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
    instanceCount: Used for instanced rendering, use 1 if you're not doing that.
    firstIndex: Used as an offset into the index buffer
    vertexOffset: used as an offset into the vertex buffer?
    firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
    */
    commandBuffer.drawIndexed(static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);

#if defined(_DEBUG)
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

    m_VK_renderFinishedSemaphores.resize(kMaxFramesInFlight);
    m_VK_imageAvailableSemaphores.resize(kMaxFramesInFlight);
    m_VK_inFlightFences.resize(kMaxFramesInFlight);

    try {
        
        
        for (size_t i = 0; i < kMaxFramesInFlight; i++) {
            m_VK_imageAvailableSemaphores[i] = m_VK_device.createSemaphore(semaphoreInfo);
            m_VK_renderFinishedSemaphores[i] = m_VK_device.createSemaphore(semaphoreInfo);
            m_VK_inFlightFences[i] = m_VK_device.createFence(fenceInfo);
        }

       
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to record command buffer!");
    }

}

void Craig::Renderer::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory) {
    QueueFamilyIndices indices = findQueueFamilies(m_VK_physicalDevice);

    vk::BufferCreateInfo bufferInfo;

    if (indices.hasDedicatedTransfer()) {
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.transferFamily.value() };
        bufferInfo.setSize(size) //Size of the triangle
            .setUsage(usage)
            .setSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndexCount(2)
            .setPQueueFamilyIndices(queueFamilyIndices);
    }
    else {
        bufferInfo.setSize(size) //Size of the triangle
            .setUsage(usage)
            .setSharingMode(vk::SharingMode::eExclusive); //Buffer's only going to be used by the graphics queue
    }

    buffer = m_VK_device.createBuffer(bufferInfo);

    vk::MemoryRequirements memRequirements;
    memRequirements = m_VK_device.getBufferMemoryRequirements(buffer);

    //Allocating memory for the vertex buffer
    vk::MemoryAllocateInfo allocInfo;
    allocInfo.setAllocationSize(memRequirements.size)
        .setMemoryTypeIndex(findMemoryType(memRequirements.memoryTypeBits, properties));
    bufferMemory = m_VK_device.allocateMemory(allocInfo);

    //Binding the memory to the buffer
    //Since the memory is allocated specifically for the vertex buffer, the offset is 0. Otherwise the offset has to be divisble by memRequirements.alignment
    m_VK_device.bindBufferMemory(buffer, bufferMemory, 0);

}

void Craig::Renderer::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {

    //Allocate a temporary command buffer
    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(m_VK_transferCommandPool)
        .setCommandBufferCount(1);

    
    std::vector<vk::CommandBuffer> commandBuffers = m_VK_device.allocateCommandBuffers(allocInfo);
    vk::CommandBuffer commandBuffer = commandBuffers[0];

    //Start recording the command buffer
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(beginInfo);

    //Copy over the data
    vk::BufferCopy copyRegion;
    copyRegion.setSize(size);
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);

    //Stop recording, we only have one command (copy)
    commandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.setCommandBufferCount(1)
        .setCommandBuffers(commandBuffer);

    m_VK_transferQueue.submit(submitInfo);
    m_VK_transferQueue.waitIdle(); //We could use a fence instead to allow multiple transfers simultaniously.

    m_VK_device.freeCommandBuffers(m_VK_transferCommandPool, commandBuffer);


}

void Craig::Renderer::createVertexBuffer() {

    vk::DeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);


    //Filling the vertex buffer in GPU memory
    void* data;
    data = m_VK_device.mapMemory(stagingBufferMemory, 0, bufferSize);
    memcpy(data, m_vertices.data(), (size_t)bufferSize);
    m_VK_device.unmapMemory(stagingBufferMemory);

    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_VK_vertexBuffer, m_VK_vertexBufferMemory);

    copyBuffer(stagingBuffer, m_VK_vertexBuffer, bufferSize);

    m_VK_device.destroyBuffer(stagingBuffer);
    m_VK_device.freeMemory(stagingBufferMemory);
}

void Craig::Renderer::createIndexBuffer() {

    vk::DeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);


    //Filling the vertex buffer in GPU memory
    void* data;
    data = m_VK_device.mapMemory(stagingBufferMemory, 0, bufferSize);
    memcpy(data, m_indices.data(), (size_t)bufferSize);
    m_VK_device.unmapMemory(stagingBufferMemory);

    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_VK_indexBuffer, m_VK_indexBufferMemory);

    copyBuffer(stagingBuffer, m_VK_indexBuffer, bufferSize);

    m_VK_device.destroyBuffer(stagingBuffer);
    m_VK_device.freeMemory(stagingBufferMemory);
}

void Craig::Renderer::drawFrame() {

    // Wait until the previous frame has finished
    m_VK_device.waitForFences(m_VK_inFlightFences[m_currentFrame], vk::True, UINT64_MAX);

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_VK_physicalDevice);
    vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    if (extent.width <= 0 || extent.height <= 0) {
        return; // Skip this frame
    }

    uint32_t imageIndex;
    VkResult nextImageResult = vkAcquireNextImageKHR(m_VK_device, m_VK_swapChain, UINT64_MAX, m_VK_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (nextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    else if (nextImageResult != VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }


    m_VK_device.resetFences(m_VK_inFlightFences[m_currentFrame]);

    

    // Record drawing commands into the command buffer
    m_VK_commandBuffers[m_currentFrame].reset();
    recordCommandBuffer(m_VK_commandBuffers[m_currentFrame], imageIndex);

    // Submit the command buffer for execution
    vk::SubmitInfo submitInfo;

    vk::Semaphore waitSemaphores[] = { m_VK_imageAvailableSemaphores[m_currentFrame]};
    vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

    submitInfo.setWaitSemaphoreCount(1)
        .setPWaitSemaphores(waitSemaphores)
        .setPWaitDstStageMask(waitStages)
        .setCommandBufferCount(1)
        .setPCommandBuffers(&m_VK_commandBuffers[m_currentFrame]);

    vk::Semaphore signalSemaphores[] = { m_VK_renderFinishedSemaphores[m_currentFrame] };
    submitInfo.setSignalSemaphoreCount(1)
        .setPSignalSemaphores(signalSemaphores);

    try {
        m_VK_graphicsQueue.submit(submitInfo, m_VK_inFlightFences[m_currentFrame]);
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


#if defined(_DEBUG)
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

#if defined(_DEBUG)
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_VK_device.destroyDescriptorPool(m_VK_imguiDescriptorPool);
#endif
    m_VK_device.destroyBuffer(m_VK_indexBuffer);
    m_VK_device.freeMemory(m_VK_indexBufferMemory);

    m_VK_device.destroyBuffer(m_VK_vertexBuffer);
    m_VK_device.freeMemory(m_VK_vertexBufferMemory);

    for (size_t i = 0; i < kMaxFramesInFlight; i++) {
        m_VK_device.destroySemaphore(m_VK_imageAvailableSemaphores[i]);
        m_VK_device.destroySemaphore(m_VK_renderFinishedSemaphores[i]);
        m_VK_device.destroyFence(m_VK_inFlightFences[i]);
    }    

    if (m_VK_transferCommandPool && m_VK_transferCommandPool != m_VK_commandPool) {
        m_VK_device.destroyCommandPool(m_VK_transferCommandPool);
    }
       
    m_VK_device.destroyCommandPool(m_VK_commandPool);

    for (auto framebuffer : m_VK_swapChainFramebuffers) {
        m_VK_device.destroyFramebuffer(framebuffer);
    }

    m_VK_device.destroyPipeline(m_VK_graphicsPipeline);
    m_VK_device.destroyPipelineLayout(m_VK_pipelineLayout);
    m_VK_device.destroyRenderPass(m_VK_renderPass);

    m_VK_device.destroyShaderModule(m_VK_vertShaderModule);
    m_VK_device.destroyShaderModule(m_VK_fragShaderModule);

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

vk::VertexInputBindingDescription Craig::Renderer::Vertex::getBindingDescription()  {
    vk::VertexInputBindingDescription bindingDescription;

    bindingDescription.setBinding(0)
        .setStride(sizeof(Vertex))
        .setInputRate(vk::VertexInputRate::eVertex);
       

    return bindingDescription;
}

std::array<vk::VertexInputAttributeDescription, 2> Craig::Renderer::Vertex::getAttributeDescriptions() {
    std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions;

    attributeDescriptions[0].setBinding(0) 
        .setLocation(0)//Location0 = POSITION0
        .setFormat(vk::Format::eR32G32Sfloat) //Not a colour, just uses the same format. Float2 = RG_float (Only 2 channels) 
        .setOffset(offsetof(Vertex, m_pos));

    attributeDescriptions[1].setBinding(0) 
        .setLocation(1)//Location1 = COLOR1 <- ps fuck american spelling.
        .setFormat(vk::Format::eR32G32B32Sfloat) //This time it IS a colour, so float3 = RGB_float
        .setOffset(offsetof(Vertex, m_color));


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