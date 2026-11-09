#include "Craig_SoundPropagation.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "Craig_CommandManager.hpp"
#include "Craig/Craig_Logger.hpp"
#include "Craig/Craig_ShaderCompilation.hpp"
#include "Craig/Craig_ResourceManager.hpp"
#include "Craig/Craig_Scene.hpp"
#include "Craig/Components/Craig_Model.hpp"
#include "Craig/Craig_AcousticMaterial.hpp"

//Acceleration structure building adapted from Sascha Willems' rayquery example
//Copyright (C) 2020-2025 Sascha Willems, MIT - https://github.com/SaschaWillems/Vulkan/tree/master/examples/rayquery

//The shader reads rays as two vec4s, so this has to stay 32 bytes
static_assert(sizeof(Craig::SoundPropagation::Ray) == 32);

CraigError Craig::SoundPropagation::init(Device* pDevice, CommandManager* pCommandManager, Scene* pScene) {
    assert(pDevice != nullptr && pCommandManager != nullptr && pScene != nullptr);

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

    collectSceneInstances(*pScene, mv_instanceSnapshot, true);
    createSharedTables();
    createPipeline();
    createResultSlots(static_cast<uint32_t>(pScene->getGameObjects().size()));
    Logger::renderer().info("Acoustic scene prepared: {} instances, {} cached models", mv_instanceSnapshot.size(), m_modelGeometry.size());
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

namespace {
    //Walks the model's nodes the same way drawNode does, and bakes each node's matrix into the positions
    void collectAcousticTriangles(const Craig::Node& node, std::vector<std::array<float, 3>>& positions, std::vector<uint32_t>& indices) {
        //Same accumulated matrix the raster shader gets pushed
        const glm::mat4 nodeMatrix = node.getWorldMatrix();

        for (const Craig::SubMesh* pMesh : node.subMeshes) {
            if (pMesh->indexCount == 0) continue;

            if (pMesh->indexCount > pMesh->m_indices.size() || pMesh->indexCount % 3 != 0 ||
                positions.size() + pMesh->m_vertices.size() > std::numeric_limits<uint32_t>::max()) {
                throw std::runtime_error("invalid acoustic triangle list");
            }

            //Submesh indices start at 0, so they get moved up past everything already in the buffer
            const uint32_t baseVertex = static_cast<uint32_t>(positions.size());
            for (const Craig::Vertex& vertex : pMesh->m_vertices) {
                const glm::vec3 position(nodeMatrix * glm::vec4(vertex.m_pos, 1.0f));
                if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) {
                    throw std::runtime_error("non-finite acoustic vertex");
                }
                positions.push_back({position.x, position.y, position.z});
            }
            for (uint32_t i = 0; i < pMesh->indexCount; i++) {
                if (pMesh->m_indices[i] >= pMesh->m_vertices.size()) {
                    throw std::runtime_error("acoustic index outside vertex buffer");
                }
                indices.push_back(baseVertex + pMesh->m_indices[i]);
            }
        }

        for (const Craig::Node* pChild : node.children) {
            collectAcousticTriangles(*pChild, positions, indices);
        }
    }

    //A scale of 0 (or NaNs) would squash the instance flat, so those get skipped
    bool validAcousticTransform(const glm::mat4& transform) {
        for (int column = 0; column < 4; column++) {
            for (int row = 0; row < 4; row++) {
                if (!std::isfinite(transform[column][row])) return false;
            }
        }
        const float determinant = glm::determinant(glm::mat3(transform));
        return std::isfinite(determinant) && determinant != 0.0f;
    }
}

void Craig::SoundPropagation::createModelGeometry(const Model& model, ModelGeometry& geometry) {
    std::vector<std::array<float, 3>> positions;
    std::vector<uint32_t> indices;
    for (const Node* pNode : model.nodes) {
        collectAcousticTriangles(*pNode, positions, indices);
    }
    if (indices.empty()) return;
    if (indices.size() / 3 > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("too many acoustic triangles");
    }

    //Reflections need to know which way the hit surface faces, and the BLAS can't give its vertices back without position fetch
    geometry.normalOffset = static_cast<uint32_t>(mv_normals.size());
    for (size_t i = 0; i < indices.size(); i += 3) {
        const glm::vec3 a(positions[indices[i]][0], positions[indices[i]][1], positions[indices[i]][2]);
        const glm::vec3 b(positions[indices[i + 1]][0], positions[indices[i + 1]][1], positions[indices[i + 1]][2]);
        const glm::vec3 c(positions[indices[i + 2]][0], positions[indices[i + 2]][1], positions[indices[i + 2]][2]);
        const glm::vec3 normal = glm::cross(b - a, c - a);
        const float area = glm::length(normal);
        //Degenerate triangles just get pointed up
        const glm::vec3 unit = area > 0.0f && std::isfinite(area) ? normal / area : glm::vec3(0.0f, 1.0f, 0.0f);
        mv_normals.push_back({unit.x, unit.y, unit.z, 0.0f});
    }

    const vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
    createBuffer(geometry.vertices, positions.size() * sizeof(positions[0]), usage, true);
    createBuffer(geometry.indices, indices.size() * sizeof(indices[0]), usage, true);
    uploadBuffer(geometry.vertices, positions.data(), positions.size() * sizeof(positions[0]));
    uploadBuffer(geometry.indices, indices.data(), indices.size() * sizeof(indices[0]));

    VkAccelerationStructureGeometryKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    triangles.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    triangles.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    triangles.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.geometry.triangles.vertexData.deviceAddress = getBufferAddress(geometry.vertices);
    triangles.geometry.triangles.vertexStride = sizeof(positions[0]);
    triangles.geometry.triangles.maxVertex = static_cast<uint32_t>(positions.size() - 1);
    triangles.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.geometry.triangles.indexData.deviceAddress = getBufferAddress(geometry.indices);
    buildAccelerationStructure(vk::AccelerationStructureTypeKHR::eBottomLevel, triangles, geometry.blas,
        static_cast<uint32_t>(indices.size() / 3));

    Logger::renderer().info("Acoustic model cached: {} ({} triangles, node transforms baked)", model.modelPath, indices.size() / 3);
}

//prepareGeometry is only true when the scene's being prepared, the per frame path can't block on building new BLASes
bool Craig::SoundPropagation::collectSceneInstances(Scene& scene, std::vector<VkAccelerationStructureInstanceKHR>& instances, bool prepareGeometry) {
    instances.clear();
    mv_instanceInfoSnapshot.clear();
    const auto& objects = scene.getGameObjects();
    ResourceManager& resources = ResourceManager::getInstance();

    for (size_t objectIndex = 0; objectIndex < objects.size(); objectIndex++) {
        GameObject* pObject = objects[objectIndex];
        const auto* pModel = pObject->getComponent<Components::Model>();
        if (!pModel || !pModel->hasModel() || !pModel->isAcousticBlocker()) continue;

        const glm::mat4 transform = pObject->GetModelMatrix();
        if (!validAcousticTransform(transform)) {
            //Only warn once when preparing, not every frame
            if (prepareGeometry) Logger::renderer().warn("Skipping acoustic instance '{}': transform is singular or non-finite", pObject->getName());
            continue;
        }
        //The instance custom index only has 24 bits
        if (objectIndex > 0xffffff) throw std::runtime_error("too many acoustic instances");

        //Only build the BLAS the first time we see a model
        const std::string& path = pModel->getModelPath();
        auto entry = m_modelGeometry.find(path);
        if (entry == m_modelGeometry.end()) {
            //A model that wasn't there when we prepared, the scene has to be reloaded to pick it up
            if (!prepareGeometry) return false;
            entry = m_modelGeometry.try_emplace(path).first;
            createModelGeometry(resources.getModel(path), entry->second);
        }
        if (!entry->second.blas.handle) continue; //No triangles

        VkAccelerationStructureInstanceKHR instance{};
        //glm indexes columns first, Vulkan's instance matrix is 3x4 rows first, so it gets flipped round
        for (int row = 0; row < 3; row++) {
            for (int column = 0; column < 4; column++) {
                instance.transform.matrix[row][column] = transform[column][row];
            }
        }
        //The shader looks up the material by instance, so the custom index is where it is in this list
        instance.instanceCustomIndex = static_cast<uint32_t>(instances.size());
        instance.mask = 0xff;
        //Negative scales flip the winding, sound doesn't care which side it hits
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = entry->second.blas.address;
        //Slots are sized for the object count at prepare time
        if (!prepareGeometry && instances.size() == m_instanceCapacity) return false;
        instances.push_back(instance);
        mv_instanceInfoSnapshot.push_back({entry->second.normalOffset, pModel->getAcousticMaterial()});
    }
    return true;
}

void Craig::SoundPropagation::createPipeline() {
    const vk::Device device = mp_Device->getLogicalDevice();

    //0 = TLAS, then 8 storage buffers: rays, results, samples, instance info, normals, materials, hits, reflections
    constexpr std::array<uint32_t, 8> storageBindings = {1, 2, 3, 4, 5, 6, 7, 8};
    std::array<vk::DescriptorSetLayoutBinding, 9> bindings{};
    bindings[0] = {0, vk::DescriptorType::eAccelerationStructureKHR, 1, vk::ShaderStageFlagBits::eCompute};
    for (uint32_t i = 0; i < storageBindings.size(); ++i) {
        bindings[i + 1] = {storageBindings[i], vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute};
    }
    m_VK_descriptorSetLayout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo().setBindings(bindings));

    const std::array<vk::DescriptorPoolSize, 2> sizes = {{
        {vk::DescriptorType::eAccelerationStructureKHR, kResultSlotCount},
        {vk::DescriptorType::eStorageBuffer, 8 * kResultSlotCount}
    }};
    m_VK_descriptorPool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo().setMaxSets(kResultSlotCount).setPoolSizes(sizes));

    const vk::PushConstantRange range(vk::ShaderStageFlagBits::eCompute, 0, sizeof(QuerySettings));
    m_VK_pipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo()
        .setSetLayouts(m_VK_descriptorSetLayout).setPushConstantRanges(range));

    const vk::ShaderModule shader = ShaderCompilation::LoadShaderModule(device, "data/shaders/SoundPropagation.spv");
    vk::PipelineShaderStageCreateInfo stage{};
    stage.setStage(vk::ShaderStageFlagBits::eCompute).setModule(shader).setPName("main");
    try {
        m_VK_pipeline = device.createComputePipeline({}, vk::ComputePipelineCreateInfo().setStage(stage).setLayout(m_VK_pipelineLayout)).value;
    }
    catch (...) {
        device.destroyShaderModule(shader);
        throw;
    }
    device.destroyShaderModule(shader);
}

void Craig::SoundPropagation::writeQueryDescriptors(const ResultSlot& slot) {
    vk::WriteDescriptorSetAccelerationStructureKHR accelerationStructure{};
    accelerationStructure.setAccelerationStructures(slot.tlas.handle);
    constexpr std::array<uint32_t, 8> storageBindings = {1, 2, 3, 4, 5, 6, 7, 8};
    const std::array<vk::DescriptorBufferInfo, 8> buffers = {{
        {slot.rays.buffer, 0, sizeof(Ray) * kMaxRayCount},
        {slot.results.buffer, 0, sizeof(uint32_t) * kMaxRayCount},
        {slot.samples.buffer, 0, VK_WHOLE_SIZE},
        {slot.instanceInfo.buffer, 0, VK_WHOLE_SIZE},
        {m_normalBuffer.buffer, 0, VK_WHOLE_SIZE},
        {m_materialBuffer.buffer, 0, VK_WHOLE_SIZE},
        {slot.hits.buffer, 0, VK_WHOLE_SIZE},
        {slot.reverb.buffer, 0, VK_WHOLE_SIZE}
    }};

    std::array<vk::WriteDescriptorSet, 9> writes{};
    writes[0].setDstSet(slot.descriptorSet).setDstBinding(0).setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
        .setDescriptorCount(1).setPNext(&accelerationStructure);
    for (uint32_t i = 0; i < buffers.size(); ++i) {
        writes[i + 1].setDstSet(slot.descriptorSet).setDstBinding(storageBindings[i]).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(buffers[i]);
    }
    mp_Device->getLogicalDevice().updateDescriptorSets(writes, {});
}

void Craig::SoundPropagation::createSharedTables() {
    //An empty scene still needs something bound
    if (mv_normals.empty()) mv_normals.push_back({0.0f, 1.0f, 0.0f, 0.0f});
    createBuffer(m_normalBuffer, mv_normals.size() * sizeof(mv_normals[0]), vk::BufferUsageFlagBits::eStorageBuffer, true);
    uploadBuffer(m_normalBuffer, mv_normals.data(), mv_normals.size() * sizeof(mv_normals[0]));

    //Two vec4s per material in the shader, absorption + scattering then transmission + padding
    std::array<std::array<float, 8>, kAcousticMaterials.size()> materials{};
    for (size_t i = 0; i < materials.size(); ++i) {
        const AcousticMaterial& material = kAcousticMaterials[i];
        materials[i] = {material.absorption[0], material.absorption[1], material.absorption[2], material.scattering,
            material.transmission[0], material.transmission[1], material.transmission[2], 0.0f};
    }
    createBuffer(m_materialBuffer, sizeof(materials), vk::BufferUsageFlagBits::eStorageBuffer, true);
    uploadBuffer(m_materialBuffer, materials.data(), sizeof(materials));
}

void Craig::SoundPropagation::recordQuery(vk::CommandBuffer commandBuffer, const ResultSlot& slot, const Request& request) {
    //A dispatch that got skipped shouldn't look like a clear path
    commandBuffer.fillBuffer(slot.results.buffer, 0, VK_WHOLE_SIZE, 0xffffffff);
    vk::MemoryBarrier2 resetBarrier{};
    resetBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer).setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader).setDstAccessMask(vk::AccessFlagBits2::eShaderWrite);
    commandBuffer.pipelineBarrier2(vk::DependencyInfo().setMemoryBarriers(resetBarrier));

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, m_VK_pipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_VK_pipelineLayout, 0, slot.descriptorSet, {});
    static_assert(sizeof(QuerySettings) == 52, "push constants have to match SoundPropagation.comp");
    //Reflection rays off means 0 of them, the direct rays still go
    const uint32_t reflectionRays = request.reverbEnabled ? request.reflectionRayCount : 0u;
    const QuerySettings settings{
        {request.listener[0], request.listener[1], request.listener[2], 0.0f},
        {request.source[0], request.source[1], request.source[2], 0.0f},
        request.rayCount, reflectionRays, request.maxBounces, request.seed,
        request.captureBounces ? 1u : 0u};
    commandBuffer.pushConstants(m_VK_pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(settings), &settings);
    //One thread does a direct ray and/or a reflection ray, so enough groups for whichever there's more of
    commandBuffer.dispatch((std::max(request.rayCount, reflectionRays) + 63) / 64, 1, 1);

    vk::MemoryBarrier2 readbackBarrier{};
    readbackBarrier.setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader).setSrcAccessMask(vk::AccessFlagBits2::eShaderWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eHost).setDstAccessMask(vk::AccessFlagBits2::eHostRead);
    commandBuffer.pipelineBarrier2(vk::DependencyInfo().setMemoryBarriers(readbackBarrier));
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

    const vk::Device device = mp_Device->getLogicalDevice();
    device.waitIdle();

    device.destroyPipeline(m_VK_pipeline);
    device.destroyPipelineLayout(m_VK_pipelineLayout);
    device.destroyDescriptorPool(m_VK_descriptorPool);
    device.destroyDescriptorSetLayout(m_VK_descriptorSetLayout);
    device.destroySemaphore(m_VK_completionTimeline);
    for (auto& slot : m_slots) {
        device.destroyCommandPool(slot.commandPool);
        destroyAccelerationStructure(slot.tlas);
        destroyBuffer(slot.instances);
        destroyBuffer(slot.scratch);
        destroyBuffer(slot.rays);
        destroyBuffer(slot.results);
        destroyBuffer(slot.instanceInfo);
        destroyBuffer(slot.samples);
        destroyBuffer(slot.hits);
        destroyBuffer(slot.reverb);
    }
    destroyBuffer(m_materialBuffer);
    destroyBuffer(m_normalBuffer);

    for (auto& [path, geometry] : m_modelGeometry) {
        destroyAccelerationStructure(geometry.blas);
        destroyBuffer(geometry.vertices);
        destroyBuffer(geometry.indices);
    }
    m_modelGeometry.clear();
    destroyBuffer(m_scratchBuffer);

    //Reset everything so init can be called again later
    *this = {};
    return CRAIG_SUCCESS;
}
