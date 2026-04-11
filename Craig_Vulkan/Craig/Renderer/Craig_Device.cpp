#include "Craig_Device.hpp"

#include <set>

#include "Craig_Swapchain.hpp"

CraigError Craig::Device::init(DeviceInitInfo& initInfo) {

	CraigError ret = CRAIG_SUCCESS;

    m_DVC_surface = initInfo.surface;
    m_DVC_instance = initInfo.instance;
    mv_DVC_deviceExtensions = initInfo.deviceExtensionsVector;

    pickPhysicalDevice();
    createLogicalDevice();
    initVMA();

	return ret;
}

//From the tutorial:
/* It has been briefly touched upon before that almost every operation in Vulkan, anything from drawing to uploading textures,
requires commands to be submitted to a queue. There are different types of queues that originate from different queue families
and each family of queues allows only a subset of commands. For example, there could be a queue family that only allows processing
of compute commands or one that only allows memory transfer related commands.*/

Craig::Device::QueueFamilyIndices Craig::Device::findQueueFamilies(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface) {
    Craig::Device::QueueFamilyIndices indices;
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
        if(!indices.presentFamily && device.getSurfaceSupportKHR(i, surface)) {
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

    // Fallback: if no dedicated transfer, use graphics (it�s implicitly transfer-capable)
    if (!indices.transferFamily && indices.graphicsFamily) {
        indices.transferFamily = indices.graphicsFamily;
    }

    return indices;
}

void Craig::Device::pickPhysicalDevice() {


    auto devices = m_DVC_instance.enumeratePhysicalDevices();
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
        printf("\nFound GPU: %s\n", props.deviceName.data());
    }
    else {
        throw std::runtime_error("failed to find a suitable GPU!");
    }


}

bool Craig::Device::isDeviceSuitable(const vk::PhysicalDevice& device) {
    Device::QueueFamilyIndices indices = Device::findQueueFamilies(device, m_DVC_surface); //Check gfx device can render and present to the screen

    bool extensionsSupported = checkDeviceExtensionSupport(device); //Check it supports extensions, especifically the swapchain extension

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        swapChainAdequate = Swapchain::isSwapChainAdequate(device, m_DVC_surface);
    }

    printf("Found graphics and presentation indices: %s\n", indices.isComplete() ? "True" : "False");
    printf("Found dedicated transfer index: %s\n", indices.hasDedicatedTransfer() ? "True" : "False");
    printf("Extensions (Like swapchain/double buffers) are supported: %s\n", extensionsSupported ? "True" : "False");
    printf("The swapchain extension is adequate for our use: %s\n", swapChainAdequate ? "True" : "False");

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}


//Here we get the list of available device extensions, and check if the required ones are present
//by removing them from a set and checking if the set is empty at the end.
bool Craig::Device::checkDeviceExtensionSupport(const vk::PhysicalDevice& device) {

    std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> requiredExtensions(mv_DVC_deviceExtensions.begin(), mv_DVC_deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

void Craig::Device::createLogicalDevice() {
    // Query the queue families that support graphics and presentation
    Device::QueueFamilyIndices indices = Device::findQueueFamilies(m_VK_physicalDevice, m_DVC_surface);

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

    vk::PhysicalDeviceVulkan13Features v13{};
    v13.setDynamicRendering(true);
    v13.setSynchronization2(true);

    //Enable the timeline semaphore feature
    vk::PhysicalDeviceTimelineSemaphoreFeatures timelineFeatures;
    timelineFeatures.setTimelineSemaphore(true);

    timelineFeatures.setPNext(&v13);

    // Fill in device creation info with queue setup and feature requirements
    vk::DeviceCreateInfo createInfo = vk::DeviceCreateInfo()
        .setQueueCreateInfos(queueCreateInfos)
        .setPEnabledFeatures(&deviceFeatures)
        .setEnabledExtensionCount(static_cast<uint32_t>(mv_DVC_deviceExtensions.size()))
        .setPpEnabledExtensionNames(mv_DVC_deviceExtensions.data())
        .setPNext(&timelineFeatures);

    // Create the logical device for the selected physical device
    m_VK_logicalDevice = m_VK_physicalDevice.createDevice(createInfo);

    // Retrieve the queue handles for rendering and presentation
    m_VK_graphicsQueue = m_VK_logicalDevice.getQueue(indices.graphicsFamily.value(), 0);
    m_VK_presentationQueue = m_VK_logicalDevice.getQueue(indices.presentFamily.value(), 0);

    if (indices.hasDedicatedTransfer()) {
        m_VK_transferQueue = m_VK_logicalDevice.getQueue(indices.transferFamily.value(), 0);
    }
    else {
        m_VK_transferQueue = m_VK_graphicsQueue;
    }
}

void Craig::Device::initVMA() {

    vk::PhysicalDeviceProperties props = m_VK_physicalDevice.getProperties();

    VmaAllocatorCreateInfo vmaCreateInfo{};
    vmaCreateInfo.instance = m_DVC_instance;
    vmaCreateInfo.physicalDevice = m_VK_physicalDevice;
    vmaCreateInfo.device = m_VK_logicalDevice;
    vmaCreateInfo.vulkanApiVersion = VK_API_VERSION_1_4;

    VmaVulkanFunctions vmaFunctions{};
    vmaFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vmaFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    vmaCreateInfo.pVulkanFunctions = &vmaFunctions;

    VkResult r = vmaCreateAllocator(&vmaCreateInfo, &m_VMA_allocator);
    if (r != VK_SUCCESS) throw std::runtime_error("vmaCreateAllocator failed");

}

void Craig::Device::createBufferVMA(
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
    if (auto idx = Device::findQueueFamilies(m_VK_physicalDevice, m_DVC_surface); idx.hasDedicatedTransfer()) {
        uint32_t q[2] = { idx.graphicsFamily.value(), idx.transferFamily.value() };
        bi.sharingMode = VK_SHARING_MODE_CONCURRENT;
        bi.queueFamilyIndexCount = 2;
        bi.pQueueFamilyIndices = q;
    }

    VkBuffer raw{};
    vmaCreateBuffer(m_VMA_allocator, &bi, &aci, &raw, &alloc, outInfo);
    buffer = vk::Buffer(raw);
}

CraigError Craig::Device::terminate() {

	CraigError ret = CRAIG_SUCCESS;

    vmaDestroyAllocator(m_VMA_allocator);
    m_VK_logicalDevice.destroy();

	return ret;
}


