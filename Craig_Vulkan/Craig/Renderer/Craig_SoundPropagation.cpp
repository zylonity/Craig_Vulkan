#include "Craig_SoundPropagation.hpp"

#include <cassert>
#include <cstring>
#include <stdexcept>

#include "Craig_CommandManager.hpp"
#include "Craig/Craig_Logger.hpp"

//The shader reads rays as two vec4s, so this has to stay 32 bytes
static_assert(sizeof(Craig::SoundPropagation::Ray) == 32);

CraigError Craig::SoundPropagation::init(Device* pDevice, CommandManager* pCommandManager) {
    assert(pDevice != nullptr && pCommandManager != nullptr);

    //No CPU fallback, if the device didn't get ray queries turned on we just don't do sound propagation
    if (!pDevice->isRayQueryEnabled()) {
        Logger::renderer().error("Sound propagation unavailable: required ray-query device features/extensions are not enabled");
        return CRAIG_FAIL;
    }
    mp_Device = pDevice;
    mp_CommandManager = pCommandManager;
    loadEntryPoints();

    //The scratch buffer address has to be a multiple of this when building acceleration structures
    const auto properties = mp_Device->getPhysicalDevice().getProperties2<
        vk::PhysicalDeviceProperties2, vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
    m_scratchAlignment = properties.get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>().minAccelerationStructureScratchOffsetAlignment;

    //Vertex positions go in as 3 floats
    const auto format = mp_Device->getPhysicalDevice().getFormatProperties(vk::Format::eR32G32B32Sfloat);
    if (!(format.bufferFeatures & vk::FormatFeatureFlagBits::eAccelerationStructureVertexBufferKHR)) {
        throw std::runtime_error("R32G32B32_SFLOAT acceleration-structure vertices unsupported");
    }

    return CRAIG_SUCCESS;
}

void Craig::SoundPropagation::loadEntryPoints() {
    const vk::Device device = mp_Device->getLogicalDevice();
    m_vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(device.getProcAddr("vkCreateAccelerationStructureKHR"));
    m_vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(device.getProcAddr("vkDestroyAccelerationStructureKHR"));
    m_vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(device.getProcAddr("vkGetAccelerationStructureBuildSizesKHR"));
    m_vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(device.getProcAddr("vkCmdBuildAccelerationStructuresKHR"));
    m_vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(device.getProcAddr("vkGetAccelerationStructureDeviceAddressKHR"));

    if (!m_vkCreateAccelerationStructureKHR || !m_vkDestroyAccelerationStructureKHR || !m_vkGetAccelerationStructureBuildSizesKHR ||
        !m_vkCmdBuildAccelerationStructuresKHR || !m_vkGetAccelerationStructureDeviceAddressKHR) {
        throw std::runtime_error("ray-query device entry points missing");
    }
}

void Craig::SoundPropagation::createBuffer(Buffer& buffer, vk::DeviceSize size, vk::BufferUsageFlags usage, bool hostVisible) {
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    if (hostVisible) {
        //These stay mapped the whole time for uploads and reading results back
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocationInfo mappedInfo{};
    mp_Device->createBufferVMA(size, usage, allocationInfo, buffer.buffer, buffer.allocation, &mappedInfo);
    if (!buffer.buffer) {
        throw std::runtime_error("ray-query buffer allocation failed");
    }
    buffer.pMapped = mappedInfo.pMappedData;
}

void Craig::SoundPropagation::uploadBuffer(Buffer& buffer, const void* pData, size_t size) {
    std::memcpy(buffer.pMapped, pData, size);
    if (vmaFlushAllocation(mp_Device->getVmaAllocator(), buffer.allocation, 0, size) != VK_SUCCESS) {
        throw std::runtime_error("ray-query upload flush failed");
    }
    //Submitting the command buffer makes the flushed writes visible to the GPU, so no barrier needed for this
}

vk::DeviceAddress Craig::SoundPropagation::getBufferAddress(const Buffer& buffer) {
    return mp_Device->getLogicalDevice().getBufferAddress(vk::BufferDeviceAddressInfo().setBuffer(buffer.buffer));
}

void Craig::SoundPropagation::destroyBuffer(Buffer& buffer) {
    if (buffer.buffer) {
        vmaDestroyBuffer(mp_Device->getVmaAllocator(), buffer.buffer, buffer.allocation);
        buffer = {};
    }
}

CraigError Craig::SoundPropagation::terminate() {
    //Never got initialised (no ray queries), nothing to clean up
    if (!mp_Device) {
        return CRAIG_SUCCESS;
    }

    mp_Device->getLogicalDevice().waitIdle();

    //Reset everything so init can be called again later
    *this = {};
    return CRAIG_SUCCESS;
}
