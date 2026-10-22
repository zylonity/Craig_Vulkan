#include "Craig_SoundPropagation.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>

//Slot/timeline setup adapted from Sascha Willems' timelinesemaphore example
//Copyright (C) 2024-2025 Sascha Willems, MIT - https://github.com/SaschaWillems/Vulkan/tree/master/examples/timelinesemaphore
//TLAS allocation adapted from his rayquery example - https://github.com/SaschaWillems/Vulkan/tree/master/examples/rayquery

void Craig::SoundPropagation::createResultSlots(uint32_t instanceCapacity) {
    const vk::Device device = mp_Device->getLogicalDevice();
    m_instanceCapacity = std::max(1u, instanceCapacity);
    mv_instanceSnapshot.reserve(m_instanceCapacity);

    //One timeline for every sound submission, each slot remembers what value it's waiting for
    vk::SemaphoreTypeCreateInfo type{};
    type.setSemaphoreType(vk::SemaphoreType::eTimeline).setInitialValue(0);
    m_VK_completionTimeline = device.createSemaphore(vk::SemaphoreCreateInfo().setPNext(&type));

    for (auto& slot : m_slots) {
        createBuffer(slot.instances, m_instanceCapacity * sizeof(VkAccelerationStructureInstanceKHR),
            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, true);
        createBuffer(slot.rays, sizeof(Request::rays), vk::BufferUsageFlagBits::eStorageBuffer, true);
        createBuffer(slot.results, sizeof(uint32_t) * kMaxRayCount,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, true);

        //The TLAS gets made once at full capacity and rebuilt in place every submission
        VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.data.deviceAddress = getBufferAddress(slot.instances);

        VkAccelerationStructureBuildGeometryInfoKHR build{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        build.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        build.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        build.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        build.geometryCount = 1;
        build.pGeometries = &geometry;

        VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        m_vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &build, &m_instanceCapacity, &sizes);
        createBuffer(slot.tlas.storage, sizes.accelerationStructureSize,
            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress);

        VkAccelerationStructureCreateInfoKHR create{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        create.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        create.buffer = slot.tlas.storage.buffer;
        create.size = sizes.accelerationStructureSize;
        VkAccelerationStructureKHR handle{};
        const VkResult result = m_vkCreateAccelerationStructureKHR(device, &create, nullptr, &handle);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("result-slot TLAS allocation failed");
        }
        slot.tlas.handle = handle;

        //The spec says scratch can be 0 bytes, but we still need an actual buffer to take an address of
        createBuffer(slot.scratch, std::max(vk::DeviceSize(1), sizes.buildScratchSize) + m_scratchAlignment - 1,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress);

        //Own pool per slot so resetting one doesn't touch a buffer that's still in flight
        slot.commandPool = device.createCommandPool(vk::CommandPoolCreateInfo()
            .setQueueFamilyIndex(mp_Device->getQueueFamilyIndices().graphicsFamily.value())
            .setFlags(vk::CommandPoolCreateFlagBits::eTransient));
        slot.commandBuffer = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo()
            .setCommandPool(slot.commandPool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1))[0];
        slot.descriptorSet = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo()
            .setDescriptorPool(m_VK_descriptorPool).setSetLayouts(m_VK_descriptorSetLayout))[0];
        writeQueryDescriptors(slot);
    }
}
