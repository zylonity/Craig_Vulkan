#include "Craig_SoundPropagation.hpp"
#include "Craig_SteamAudioSampling.hpp"
#include "Craig_CommandManager.hpp"

#include <stdexcept>

#include "Craig/Craig_Logger.hpp"
#include "Craig/Craig_ShaderCompilation.hpp"
#include "Craig/Craig_Scene.hpp"

//The GPU half of the pathing bake, Steam Audio's probe visibility test traced with ray queries
//Everything's made and thrown away in here, a bake only happens when the probes change

Craig::SoundPropagation::VisibilityBake Craig::SoundPropagation::bakeProbeVisibility(Scene& scene, const std::vector<ProbeSphere>& probes,
    const std::vector<SoundPathing::Pair>& pairs, uint32_t samples, float radius, float threshold) {
    VisibilityBake bake;
    bake.visible.assign(pairs.size(), 0);
    if (!mp_Device || pairs.empty()) return bake;
    const vk::Device device = mp_Device->getLogicalDevice();

    //A TLAS of where everything is right now, its own one so none of the in flight slots get touched
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    if (!collectSceneInstances(scene, instances, false)) throw std::runtime_error("scene changed since the sound geometry was prepared");
    if (instances.empty()) {
        //Nothing to block anything, every pair can see every other
        std::fill(bake.visible.begin(), bake.visible.end(), 1);
        return bake;
    }
    Buffer instanceBuffer;
    createBuffer(instanceBuffer, instances.size() * sizeof(instances[0]),
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, true);
    uploadBuffer(instanceBuffer, instances.data(), instances.size() * sizeof(instances[0]));
    VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.data.deviceAddress = getBufferAddress(instanceBuffer);
    AccelerationStructure tlas;
    buildAccelerationStructure(vk::AccelerationStructureTypeKHR::eTopLevel, geometry, tlas, static_cast<uint32_t>(instances.size()));

    //Inputs, all small enough to just stay mapped
    std::vector<std::array<float, 4>> probeData(probes.size());
    for (size_t i = 0; i < probes.size(); ++i) probeData[i] = {probes[i].centre.x, probes[i].centre.y, probes[i].centre.z, probes[i].radius};
    std::vector<std::array<float, 4>> sampleData(std::max(1u, samples));
    fillSteamAudioSphereVolumeSamples(sampleData);
    Buffer probeBuffer, pairBuffer, sampleBuffer, visibleBuffer;
    createBuffer(probeBuffer, probeData.size() * sizeof(probeData[0]), vk::BufferUsageFlagBits::eStorageBuffer, true);
    createBuffer(pairBuffer, pairs.size() * sizeof(pairs[0]), vk::BufferUsageFlagBits::eStorageBuffer, true);
    createBuffer(sampleBuffer, sampleData.size() * sizeof(sampleData[0]), vk::BufferUsageFlagBits::eStorageBuffer, true);
    createBuffer(visibleBuffer, pairs.size() * sizeof(uint32_t), vk::BufferUsageFlagBits::eStorageBuffer, true);
    uploadBuffer(probeBuffer, probeData.data(), probeData.size() * sizeof(probeData[0]));
    uploadBuffer(pairBuffer, pairs.data(), pairs.size() * sizeof(pairs[0]));
    uploadBuffer(sampleBuffer, sampleData.data(), sampleData.size() * sizeof(sampleData[0]));

    //0 = TLAS, then probes, pairs, samples, visible
    std::array<vk::DescriptorSetLayoutBinding, 5> bindings{};
    bindings[0] = {0, vk::DescriptorType::eAccelerationStructureKHR, 1, vk::ShaderStageFlagBits::eCompute};
    for (uint32_t i = 1; i < 5; ++i) bindings[i] = {i, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute};
    const vk::DescriptorSetLayout layout = device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo().setBindings(bindings));
    const std::array<vk::DescriptorPoolSize, 2> poolSizes = {{{vk::DescriptorType::eAccelerationStructureKHR, 1}, {vk::DescriptorType::eStorageBuffer, 4}}};
    const vk::DescriptorPool pool = device.createDescriptorPool(vk::DescriptorPoolCreateInfo().setMaxSets(1).setPoolSizes(poolSizes));
    const vk::DescriptorSet set = device.allocateDescriptorSets(vk::DescriptorSetAllocateInfo().setDescriptorPool(pool).setSetLayouts(layout))[0];
    vk::WriteDescriptorSetAccelerationStructureKHR tlasWrite{};
    tlasWrite.setAccelerationStructures(tlas.handle);
    const std::array<vk::DescriptorBufferInfo, 4> bufferInfo = {{
        {probeBuffer.buffer, 0, VK_WHOLE_SIZE}, {pairBuffer.buffer, 0, VK_WHOLE_SIZE},
        {sampleBuffer.buffer, 0, VK_WHOLE_SIZE}, {visibleBuffer.buffer, 0, VK_WHOLE_SIZE}}};
    std::array<vk::WriteDescriptorSet, 5> writes{};
    writes[0].setDstSet(set).setDstBinding(0).setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR).setDescriptorCount(1).setPNext(&tlasWrite);
    for (uint32_t i = 0; i < 4; ++i) {
        writes[i + 1].setDstSet(set).setDstBinding(i + 1).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(bufferInfo[i]);
    }
    device.updateDescriptorSets(writes, {});

    //Has to match ProbeVisibility.comp's push constants
    struct VisibilitySettings {
        uint32_t pairCount;
        uint32_t sampleCount;
        float radius;
        float threshold;
    };
    const VisibilitySettings settings{static_cast<uint32_t>(pairs.size()), samples, radius, threshold};
    const vk::PushConstantRange range(vk::ShaderStageFlagBits::eCompute, 0, sizeof(settings));
    const vk::PipelineLayout pipelineLayout = device.createPipelineLayout(vk::PipelineLayoutCreateInfo().setSetLayouts(layout).setPushConstantRanges(range));
    const vk::ShaderModule shader = ShaderCompilation::LoadShaderModule(device, "data/shaders/ProbeVisibility.spv");
    const vk::PipelineShaderStageCreateInfo stage({}, vk::ShaderStageFlagBits::eCompute, shader, "main");
    const vk::Pipeline pipeline = device.createComputePipeline({}, vk::ComputePipelineCreateInfo().setStage(stage).setLayout(pipelineLayout)).value;
    device.destroyShaderModule(shader);

    const vk::QueryPool timestamps = m_timestampsSupported ?
        device.createQueryPool(vk::QueryPoolCreateInfo().setQueryType(vk::QueryType::eTimestamp).setQueryCount(2)) : vk::QueryPool{};

    //Waits for the queue, fine for a bake, never for the per frame path
    const vk::CommandBuffer commandBuffer = mp_CommandManager->buffer_beginSingleTimeCommandsGFX();
    if (timestamps) {
        commandBuffer.resetQueryPool(timestamps, 0, 2);
        commandBuffer.writeTimestamp2(vk::PipelineStageFlagBits2::eTopOfPipe, timestamps, 0);
    }
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, set, {});
    commandBuffer.pushConstants(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(settings), &settings);
    commandBuffer.dispatch((settings.pairCount + 63) / 64, 1, 1);
    if (timestamps) commandBuffer.writeTimestamp2(vk::PipelineStageFlagBits2::eComputeShader, timestamps, 1);
    vk::MemoryBarrier2 readback{};
    readback.setSrcStageMask(vk::PipelineStageFlagBits2::eComputeShader).setSrcAccessMask(vk::AccessFlagBits2::eShaderWrite)
        .setDstStageMask(vk::PipelineStageFlagBits2::eHost).setDstAccessMask(vk::AccessFlagBits2::eHostRead);
    commandBuffer.pipelineBarrier2(vk::DependencyInfo().setMemoryBarriers(readback));
    mp_CommandManager->buffer_endSingleTimeCommandsGFX(commandBuffer);

    if (timestamps) {
        std::array<uint64_t, 2> ticks{};
        if (device.getQueryPoolResults(timestamps, 0, 2, sizeof(ticks), ticks.data(), sizeof(uint64_t), vk::QueryResultFlagBits::e64) == vk::Result::eSuccess) {
            bake.gpuMs = static_cast<double>(ticks[1] - ticks[0]) * m_timestampPeriodNs / 1e6;
        }
        device.destroyQueryPool(timestamps);
    }
    if (vmaInvalidateAllocation(mp_Device->getVmaAllocator(), visibleBuffer.allocation, 0, VK_WHOLE_SIZE) != VK_SUCCESS) {
        throw std::runtime_error("probe visibility readback invalidation failed");
    }
    const auto* visible = static_cast<const uint32_t*>(visibleBuffer.pMapped);
    for (size_t i = 0; i < pairs.size(); ++i) bake.visible[i] = visible[i] != 0;

    device.destroyPipeline(pipeline);
    device.destroyPipelineLayout(pipelineLayout);
    device.destroyDescriptorPool(pool);
    device.destroyDescriptorSetLayout(layout);
    destroyBuffer(probeBuffer);
    destroyBuffer(pairBuffer);
    destroyBuffer(sampleBuffer);
    destroyBuffer(visibleBuffer);
    destroyAccelerationStructure(tlas);
    destroyBuffer(instanceBuffer);
    Logger::renderer().info("GPU probe visibility: {} probes, {} pairs, {} samples, {:.3f} ms", probes.size(), pairs.size(), samples, bake.gpuMs);
    return bake;
}
