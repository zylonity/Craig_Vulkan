#include "Craig_SoundPropagation.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <stdexcept>

#include "Craig_CommandManager.hpp"
#include "Craig/Craig_Logger.hpp"

//Acceleration structure building adapted from Sascha Willems' rayquery example
//Copyright (C) 2020-2025 Sascha Willems, MIT - https://github.com/SaschaWillems/Vulkan/tree/master/examples/rayquery

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

void Craig::SoundPropagation::buildAccelerationStructure(vk::AccelerationStructureTypeKHR type,
    const VkAccelerationStructureGeometryKHR& geometry, AccelerationStructure& accelerationStructure, uint32_t primitiveCount) {
    const vk::Device device = mp_Device->getLogicalDevice();

    //What we're building
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    buildInfo.type = static_cast<VkAccelerationStructureTypeKHR>(type);
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    //Ask the driver how much memory it needs for the structure itself and for scratch space while building
    VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    m_vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizes);

    createBuffer(accelerationStructure.storage, sizes.accelerationStructureSize,
        vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress);

    VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
    createInfo.type = buildInfo.type;
    createInfo.buffer = accelerationStructure.storage.buffer;
    createInfo.size = sizes.accelerationStructureSize;

    VkAccelerationStructureKHR rawHandle{};
    const VkResult result = m_vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &rawHandle);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("acceleration structure creation failed: " + vk::to_string(vk::Result(result)));
    }
    accelerationStructure.handle = rawHandle;

    //Extra alignment - 1 bytes on the end so we can push the address up to the alignment and still have the full size after it
    createBuffer(m_scratchBuffer, std::max(vk::DeviceSize(1), sizes.buildScratchSize) + m_scratchAlignment - 1,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);
    const vk::DeviceAddress scratchAddress = getBufferAddress(m_scratchBuffer);
    buildInfo.scratchData.deviceAddress = (scratchAddress + m_scratchAlignment - 1) & ~(m_scratchAlignment - 1);
    buildInfo.dstAccelerationStructure = accelerationStructure.handle;

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = primitiveCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;

    vk::CommandBuffer commandBuffer = mp_CommandManager->buffer_beginSingleTimeCommandsGFX();
    m_vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pRange);

    //Anything after this (another build or a compute shader) has to wait for the build to finish writing
    vk::MemoryBarrier2 barrier{};
    barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR)
        .setSrcAccessMask(vk::AccessFlagBits2::eAccelerationStructureWriteKHR)
        .setDstStageMask(vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits2::eComputeShader)
        .setDstAccessMask(vk::AccessFlagBits2::eAccelerationStructureReadKHR);
    commandBuffer.pipelineBarrier2(vk::DependencyInfo().setMemoryBarriers(barrier));

    //This waits for the queue to finish, so the scratch buffer's safe to get rid of straight after
    mp_CommandManager->buffer_endSingleTimeCommandsGFX(commandBuffer);
    destroyBuffer(m_scratchBuffer);

    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.accelerationStructure = accelerationStructure.handle;
    accelerationStructure.address = m_vkGetAccelerationStructureDeviceAddressKHR(device, &addressInfo);

    Logger::renderer().info("Ray-query {} built: {} bytes storage, {} bytes scratch", vk::to_string(type), sizes.accelerationStructureSize, sizes.buildScratchSize);
}

void Craig::SoundPropagation::destroyAccelerationStructure(AccelerationStructure& accelerationStructure) {
    if (accelerationStructure.handle) {
        m_vkDestroyAccelerationStructureKHR(mp_Device->getLogicalDevice(), accelerationStructure.handle, nullptr);
    }
    destroyBuffer(accelerationStructure.storage);
    accelerationStructure = {};
}

CraigError Craig::SoundPropagation::runTriangleTest() {
    //One triangle facing +Z, sat at z = 0 around the origin
    const std::array<std::array<float, 3>, 3> positions = {{
        {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}
    }};
    const std::array<uint32_t, 3> indices = {0, 1, 2};

    const vk::BufferUsageFlags inputUsage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    createBuffer(m_testVertices, sizeof(positions), inputUsage, true);
    createBuffer(m_testIndices, sizeof(indices), inputUsage, true);
    uploadBuffer(m_testVertices, positions.data(), sizeof(positions));
    uploadBuffer(m_testIndices, indices.data(), sizeof(indices));

    //Bottom level, the actual triangles
    VkAccelerationStructureGeometryKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    triangles.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    triangles.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    triangles.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.geometry.triangles.vertexData.deviceAddress = getBufferAddress(m_testVertices);
    triangles.geometry.triangles.vertexStride = sizeof(positions[0]);
    triangles.geometry.triangles.maxVertex = 2;
    triangles.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.geometry.triangles.indexData.deviceAddress = getBufferAddress(m_testIndices);
    buildAccelerationStructure(vk::AccelerationStructureTypeKHR::eBottomLevel, triangles, m_testBLAS, 1);

    //Top level, one instance of that triangle with no transform
    VkAccelerationStructureInstanceKHR instance{};
    instance.transform.matrix[0][0] = 1.0f;
    instance.transform.matrix[1][1] = 1.0f;
    instance.transform.matrix[2][2] = 1.0f;
    instance.mask = 0xff;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = m_testBLAS.address;
    createBuffer(m_testInstances, sizeof(instance), inputUsage, true);
    uploadBuffer(m_testInstances, &instance, sizeof(instance));

    VkAccelerationStructureGeometryKHR instances{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    instances.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    instances.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instances.geometry.instances.data.deviceAddress = getBufferAddress(m_testInstances);
    buildAccelerationStructure(vk::AccelerationStructureTypeKHR::eTopLevel, instances, m_testTLAS, 1);

    return CRAIG_SUCCESS;
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

    destroyAccelerationStructure(m_testTLAS);
    destroyAccelerationStructure(m_testBLAS);
    destroyBuffer(m_testInstances);
    destroyBuffer(m_testIndices);
    destroyBuffer(m_testVertices);
    destroyBuffer(m_scratchBuffer);

    //Reset everything so init can be called again later
    *this = {};
    return CRAIG_SUCCESS;
}
