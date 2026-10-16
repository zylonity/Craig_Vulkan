#include "Craig_Device.hpp"

#include <set>
#include <cstring>

#include "Craig_Swapchain.hpp"
#include "Craig/Craig_Logger.hpp"

CraigError Craig::Device::init(DeviceInitInfo& initInfo) {

	CraigError ret = CRAIG_SUCCESS;

    m_DVC_surface = initInfo.surface;
    m_DVC_instance = initInfo.instance;
    mv_DVC_deviceExtensions = initInfo.deviceExtensionsVector;

    pickPhysicalDevice();
    //Find them once here and keep them, createBufferVMA used to look them up for every single buffer
    m_DVC_queueFamilyIndices = findQueueFamilies(m_VK_physicalDevice, m_DVC_surface);
    queryRayQuerySupport();
    createLogicalDevice();
    initVMA();

    Craig::Logger::renderer().info("Device ready");

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
    //Used to be a range for with i++ at the bottom, so the continue below skipped the i++ and every index after it was off by one
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        const auto& queueFamily = queueFamilies[i];
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
        Craig::Logger::renderer().critical("No Vulkan-compatible GPUs found, check your drivers");
        throw std::runtime_error("No Vulkan-compatible GPUs found.");
    }

    Craig::Logger::renderer().info("Found {} GPU(s), checking which ones work", devices.size());

    for (const auto& device : devices) {
        if (!isDeviceSuitable(device)) {
            Craig::Logger::renderer().info("Skipping {}, it's missing something we need (see the debug lines above)", device.getProperties().deviceName.data());
            continue;
        }

        // On macOS the GPU shows up once for each driver (MoltenVK and KosmicKrisp), we want KosmicKrisp if it's there
        const std::string driverName = device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDriverProperties>()
            .get<vk::PhysicalDeviceDriverProperties>().driverName.data();

        if (driverName == "KosmicKrisp") {
            m_VK_physicalDevice = device;
            break;
        }
        if (!m_VK_physicalDevice) {
            m_VK_physicalDevice = device;
        }
    }

    if (m_VK_physicalDevice) {
        const auto props = m_VK_physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceDriverProperties>();
        const vk::PhysicalDeviceProperties& properties = props.get<vk::PhysicalDeviceProperties2>().properties;
        Craig::Logger::renderer().info("Using GPU: {} ({}, driver: {}, Vulkan {}.{}.{})",
            properties.deviceName.data(),
            vk::to_string(properties.deviceType),
            props.get<vk::PhysicalDeviceDriverProperties>().driverName.data(),
            VK_API_VERSION_MAJOR(properties.apiVersion), VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion));
    }
    else {
        Craig::Logger::renderer().critical("None of the GPUs have what we need (graphics + present queues, swapchain support and the device extensions)");
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

    const vk::PhysicalDeviceProperties properties = device.getProperties();
    Craig::Logger::renderer().debug("Checking {} ({})", properties.deviceName.data(), vk::to_string(properties.deviceType));
    Craig::Logger::renderer().debug("  Found graphics and presentation indices: {}", indices.isComplete());
    Craig::Logger::renderer().debug("  Found dedicated transfer index: {}", indices.hasDedicatedTransfer());
    Craig::Logger::renderer().debug("  Extensions (Like swapchain/double buffers) are supported: {}", extensionsSupported);
    Craig::Logger::renderer().debug("  The swapchain extension is adequate for our use: {}", swapChainAdequate);

    //The instance and VMA are both on 1.4 already, so the device has to be too
    const bool apiSupported = properties.apiVersion >= VK_API_VERSION_1_4;
    Craig::Logger::renderer().debug("  Vulkan 1.4 device API supported: {}", apiSupported);

    return apiSupported && indices.isComplete() && extensionsSupported && swapChainAdequate;
}


//Here we get the list of available device extensions, and check if the required ones are present
//by removing them from a set and checking if the set is empty at the end.
bool Craig::Device::checkDeviceExtensionSupport(const vk::PhysicalDevice& device) {

    std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> requiredExtensions(mv_DVC_deviceExtensions.begin(), mv_DVC_deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    // whatever's left over is missing
    for (const std::string& missingExtension : requiredExtensions) {
        Craig::Logger::renderer().debug("  {} doesn't have {}", device.getProperties().deviceName.data(), missingExtension);
    }

    return requiredExtensions.empty();
}

void Craig::Device::queryRayQuerySupport() {
    m_DVC_rayQuerySupport = {};

    //Check the extensions first
    for (const vk::ExtensionProperties& extension : m_VK_physicalDevice.enumerateDeviceExtensionProperties()) {
        if (std::strcmp(extension.extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0) {
            m_DVC_rayQuerySupport.accelerationStructureExtension = true;
        }
        if (std::strcmp(extension.extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0) {
            m_DVC_rayQuerySupport.rayQueryExtension = true;
        }
        if (std::strcmp(extension.extensionName, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) == 0) {
            m_DVC_rayQuerySupport.deferredHostOperationsExtension = true;
        }
    }

    //Then the features, chained off the 1.2 struct
    vk::PhysicalDeviceFeatures2 features{};
    vk::PhysicalDeviceVulkan12Features v12{};
    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructure{};
    vk::PhysicalDeviceRayQueryFeaturesKHR rayQuery{};
    features.setPNext(&v12);

    //Only chain the extension structs if the driver actually has the extension
    if (m_DVC_rayQuerySupport.accelerationStructureExtension) {
        accelerationStructure.setPNext(v12.pNext);
        v12.setPNext(&accelerationStructure);
    }
    if (m_DVC_rayQuerySupport.rayQueryExtension) {
        rayQuery.setPNext(v12.pNext);
        v12.setPNext(&rayQuery);
    }
    m_VK_physicalDevice.getFeatures2(&features);

    m_DVC_rayQuerySupport.bufferDeviceAddress = v12.bufferDeviceAddress;
    m_DVC_rayQuerySupport.accelerationStructure = accelerationStructure.accelerationStructure;
    m_DVC_rayQuerySupport.rayQuery = rayQuery.rayQuery;

    const auto queueFamilies = m_VK_physicalDevice.getQueueFamilyProperties();
    m_DVC_rayQuerySupport.graphicsQueueCompute = static_cast<bool>(
        queueFamilies[m_DVC_queueFamilyIndices.graphicsFamily.value()].queueFlags & vk::QueueFlagBits::eCompute);

    Craig::Logger::renderer().info("RT extensions supported: acceleration structure {}, ray query {}, deferred host operations {}",
        m_DVC_rayQuerySupport.accelerationStructureExtension, m_DVC_rayQuerySupport.rayQueryExtension,
        m_DVC_rayQuerySupport.deferredHostOperationsExtension);
    Craig::Logger::renderer().info("RT features supported: buffer device address {}, acceleration structure {}, ray query {}, graphics queue compute {}",
        m_DVC_rayQuerySupport.bufferDeviceAddress, m_DVC_rayQuerySupport.accelerationStructure,
        m_DVC_rayQuerySupport.rayQuery, m_DVC_rayQuerySupport.graphicsQueueCompute);
}

void Craig::Device::createLogicalDevice() {
    //Found in init, before we get here
    Device::QueueFamilyIndices& indices = m_DVC_queueFamilyIndices;

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

    std::vector<const char*> enabledExtensions = mv_DVC_deviceExtensions;

#if defined(__APPLE__)
    // MoltenVK has the portability subset and it has to be enabled if it's there, KosmicKrisp doesn't have it at all
    // (spelled out since the #define for it is in vulkan_beta.h)
    constexpr const char* kPortabilitySubsetExtension = "VK_KHR_portability_subset";
    for (const vk::ExtensionProperties& extension : m_VK_physicalDevice.enumerateDeviceExtensionProperties()) {
        if (std::strcmp(extension.extensionName, kPortabilitySubsetExtension) == 0) {
            enabledExtensions.push_back(kPortabilitySubsetExtension);
            break;
        }
    }
#endif

    // Fill in device creation info with queue setup and feature requirements
    vk::DeviceCreateInfo createInfo = vk::DeviceCreateInfo()
        .setQueueCreateInfos(queueCreateInfos)
        .setPEnabledFeatures(&deviceFeatures)
        .setPEnabledExtensionNames(enabledExtensions)
        .setPNext(&timelineFeatures);

    for (const char* extension : enabledExtensions) {
        Craig::Logger::renderer().debug("Device extension on: {}", extension);
    }

    // Create the logical device for the selected physical device
    m_VK_logicalDevice = m_VK_physicalDevice.createDevice(createInfo);

    Craig::Logger::renderer().info("Queue families: graphics {}, present {}, transfer {}{}",
        indices.graphicsFamily.value(), indices.presentFamily.value(), indices.transferFamily.value(),
        indices.hasDedicatedTransfer() ? " (dedicated)" : " (shared with graphics)");

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
    if (r != VK_SUCCESS) {
        Craig::Logger::renderer().critical("vmaCreateAllocator failed: {}", vk::to_string(vk::Result(r)));
        throw std::runtime_error("vmaCreateAllocator failed");
    }

    Craig::Logger::renderer().debug("VMA allocator ready");

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

    //This array used to live inside the if, so the pointer was dead by the time vmaCreateBuffer read it
    //It has to stay alive until vmaCreateBuffer returns
    uint32_t queueFamilies[2] = {
        m_DVC_queueFamilyIndices.graphicsFamily.value(), m_DVC_queueFamilyIndices.transferFamily.value()
    };

    //If the transfer queue is its own family the buffer gets shared between both
    if (m_DVC_queueFamilyIndices.hasDedicatedTransfer()) {
        bi.sharingMode = VK_SHARING_MODE_CONCURRENT;
        bi.queueFamilyIndexCount = 2;
        bi.pQueueFamilyIndices = queueFamilies;
    }

    VkBuffer raw{};
    // this used to fail silently and hand back a null buffer, fuck that
    const VkResult result = vmaCreateBuffer(m_VMA_allocator, &bi, &aci, &raw, &alloc, outInfo);
    if (result != VK_SUCCESS) {
        Craig::Logger::renderer().error("vmaCreateBuffer failed for a {} byte buffer: {}", size, vk::to_string(vk::Result(result)));
    }
    buffer = vk::Buffer(raw);
}

CraigError Craig::Device::terminate() {

	CraigError ret = CRAIG_SUCCESS;

    vmaDestroyAllocator(m_VMA_allocator);
    m_VK_logicalDevice.destroy();
    Craig::Logger::renderer().debug("Device and VMA destroyed");

	return ret;
}


