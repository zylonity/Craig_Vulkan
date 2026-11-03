#include "Craig_SoundPropagation.hpp"
#include "Craig_SteamAudioSampling.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <glm/glm.hpp>

#include "Craig/Craig_Scene.hpp"
#include "Craig/Craig_Logger.hpp"
#include "Craig/Craig_AcousticMaterial.hpp"

//Slot/timeline setup adapted from Sascha Willems' timelinesemaphore example
//Copyright (C) 2024-2025 Sascha Willems, MIT - https://github.com/SaschaWillems/Vulkan/tree/master/examples/timelinesemaphore
//TLAS allocation adapted from his rayquery example - https://github.com/SaschaWillems/Vulkan/tree/master/examples/rayquery

void Craig::SoundPropagation::createResultSlots(uint32_t instanceCapacity) {
    const vk::Device device = mp_Device->getLogicalDevice();
    m_instanceCapacity = std::max(1u, instanceCapacity);
    mv_instanceSnapshot.reserve(m_instanceCapacity);
    mv_instanceInfoSnapshot.reserve(m_instanceCapacity);

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
        createBuffer(slot.instanceInfo, m_instanceCapacity * sizeof(InstanceInfo), vk::BufferUsageFlagBits::eStorageBuffer, true);
        createBuffer(slot.samples, sizeof(std::array<float, 4>) * (kMaxReflectionRayCount + kSteamDiffuseSamples), vk::BufferUsageFlagBits::eStorageBuffer, true);
        createBuffer(slot.hits, sizeof(std::array<float, 4>) * kMaxReflectionRayCount * kReverbBounces, vk::BufferUsageFlagBits::eStorageBuffer, true);
        createBuffer(slot.reverb, sizeof(std::array<float, 4>) * kMaxReflectionRayCount * kReverbBounces, vk::BufferUsageFlagBits::eStorageBuffer, true);

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

void Craig::SoundPropagation::setContext(uint64_t sceneGeneration, uint64_t sourceId) {
    stop();
    m_sceneGeneration = sceneGeneration;
    m_sourceId = sourceId;
    m_acceptRequests = true;
}

void Craig::SoundPropagation::stop() {
    m_acceptRequests = false;
    //Bumped even if the scene/source IDs stay the same, otherwise a pause and resume could let old results back in
    m_contextVersion++;
    m_latestSequence.reset();
    if (m_readyResult) m_statistics.discarded++;
    m_readyResult.reset();
}

Craig::SoundPropagation::SubmitStatus Craig::SoundPropagation::trySubmit(Scene& scene, const Request& request) {
    assert(request.rayCount > 0 && request.rayCount <= kMaxRayCount);
    assert(request.reflectionRayCount <= kMaxReflectionRayCount && request.maxBounces <= kReverbBounces);
    m_statistics.requested++;
    collectCompleted();

    if (!m_acceptRequests || !m_VK_completionTimeline || request.sceneGeneration != m_sceneGeneration || request.sourceId != m_sourceId) {
        m_statistics.skipped++;
        return SubmitStatus::Inactive;
    }

    //If all three are still on the GPU we just skip this frame, we never wait
    auto available = std::find_if(m_slots.begin(), m_slots.end(), [](const ResultSlot& slot) { return !slot.inFlight; });
    if (available == m_slots.end()) {
        m_statistics.skipped++;
        return SubmitStatus::Busy;
    }

    if (!collectSceneInstances(scene, mv_instanceSnapshot, false)) {
        m_statistics.skipped++;
        return SubmitStatus::GeometryNotReady;
    }

    //Everything was allocated up front, and only a finished slot gets written to
    submitSlot(*available, request);
    m_statistics.submitted++;
    return SubmitStatus::Submitted;
}

void Craig::SoundPropagation::submitSlot(ResultSlot& slot, const Request& request) {
    const vk::Device device = mp_Device->getLogicalDevice();
    if (!mv_instanceSnapshot.empty()) {
        uploadBuffer(slot.instances, mv_instanceSnapshot.data(), mv_instanceSnapshot.size() * sizeof(mv_instanceSnapshot[0]));
        uploadBuffer(slot.instanceInfo, mv_instanceInfoSnapshot.data(), mv_instanceInfoSnapshot.size() * sizeof(mv_instanceInfoSnapshot[0]));
    }
    uploadBuffer(slot.rays, request.rays.data(), sizeof(Ray) * request.rayCount);
    if (slot.samplingRayCount != request.reflectionRayCount) {
        //The sphere directions depend on how many rays there are, only remade when that changes
        //Safe to write since only a finished slot gets here
        std::array<std::array<float, 4>, kMaxReflectionRayCount + kSteamDiffuseSamples> samples{};
        fillSteamAudioSamples(std::span(samples).first(request.reflectionRayCount), std::span(samples).last(kSteamDiffuseSamples));
        uploadBuffer(slot.samples, samples.data(), sizeof(samples));
        slot.samplingRayCount = request.reflectionRayCount;
    }

    device.resetCommandPool(slot.commandPool);
    slot.commandBuffer.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    //Rebuild the TLAS with where everything is this frame, the BLASes stay the same
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
    build.dstAccelerationStructure = slot.tlas.handle;
    const vk::DeviceAddress scratch = getBufferAddress(slot.scratch);
    build.scratchData.deviceAddress = (scratch + m_scratchAlignment - 1) & ~(m_scratchAlignment - 1);

    VkAccelerationStructureBuildRangeInfoKHR range{};
    range.primitiveCount = static_cast<uint32_t>(mv_instanceSnapshot.size());
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &range;
    m_vkCmdBuildAccelerationStructuresKHR(slot.commandBuffer, 1, &build, &pRange);

    vk::MemoryBarrier2 barrier{};
    barrier.setSrcStageMask(vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR)
        .setSrcAccessMask(vk::AccessFlagBits2::eAccelerationStructureWriteKHR)
        .setDstStageMask(vk::PipelineStageFlagBits2::eComputeShader).setDstAccessMask(vk::AccessFlagBits2::eAccelerationStructureReadKHR);
    slot.commandBuffer.pipelineBarrier2(vk::DependencyInfo().setMemoryBarriers(barrier));

    recordQuery(slot.commandBuffer, slot, request);
    slot.commandBuffer.end();

    //Signals the timeline when it's done, nothing waits on it, we just check the value next frame
    const uint64_t completion = m_lastSubmissionValue + 1;
    vk::CommandBufferSubmitInfo command{};
    command.setCommandBuffer(slot.commandBuffer);
    vk::SemaphoreSubmitInfo signal{};
    signal.setSemaphore(m_VK_completionTimeline).setValue(completion).setStageMask(vk::PipelineStageFlagBits2::eAllCommands);
    vk::SubmitInfo2 submit{};
    submit.setCommandBufferInfos(command).setSignalSemaphoreInfos(signal);
    mp_Device->getGraphicsQueue().submit2(submit);

    m_lastSubmissionValue = completion;
    slot.request = request;
    slot.completionValue = completion;
    slot.contextVersion = m_contextVersion;
    slot.inFlight = true;
}

void Craig::SoundPropagation::collectCompleted() {
    if (!m_VK_completionTimeline) return;

    const uint64_t completed = mp_Device->getLogicalDevice().getSemaphoreCounterValue(m_VK_completionTimeline);
    for (auto& slot : m_slots) {
        if (!slot.inFlight || slot.completionValue > completed) continue;

        slot.inFlight = false;
        m_statistics.completed++;

        //Old context, or older than something we've already got
        if (!m_acceptRequests || slot.contextVersion != m_contextVersion ||
            (m_latestSequence && slot.request.sequenceId <= *m_latestSequence)) {
            m_statistics.discarded++;
            continue;
        }

        if (vmaInvalidateAllocation(mp_Device->getVmaAllocator(), slot.results.allocation, 0, VK_WHOLE_SIZE) != VK_SUCCESS) {
            throw std::runtime_error("result-slot readback invalidation failed");
        }

        Result result{};
        buildArrivals(slot, result);
        result.reverbEnabled = slot.request.reverbEnabled;
        if (result.reverbEnabled) {
            if (vmaInvalidateAllocation(mp_Device->getVmaAllocator(), slot.reverb.allocation, 0, VK_WHOLE_SIZE) != VK_SUCCESS) {
                throw std::runtime_error("reverb-slot readback invalidation failed");
            }
            //Add every bounce's energy into the 10 ms bin it lands in
            const auto* records = static_cast<const std::array<float, 4>*>(slot.reverb.pMapped);
            for (uint32_t i = 0; i < slot.request.reflectionRayCount * kReverbBounces; ++i) {
                const auto& record = records[i];
                if (!std::isfinite(record[3]) || record[3] < 0.0f || record[3] >= kReverbBins * kReverbBinSeconds) {
                    throw std::runtime_error("invalid reverb delay");
                }
                const uint32_t bin = std::min(kReverbBins - 1, static_cast<uint32_t>(record[3] / kReverbBinSeconds));
                for (uint32_t band = 0; band < 3; ++band) {
                    if (!std::isfinite(record[band]) || record[band] < 0.0f) throw std::runtime_error("invalid reverb energy");
                    result.reverbEnergy[bin][band] += record[band];
                }
            }
        }
        if (slot.request.captureBounces) {
            if (vmaInvalidateAllocation(mp_Device->getVmaAllocator(), slot.hits.allocation, 0, VK_WHOLE_SIZE) != VK_SUCCESS) {
                throw std::runtime_error("result-slot readback invalidation failed");
            }
            const auto* hits = static_cast<const std::array<float, 4>*>(slot.hits.pMapped);
            result.bounces.assign(hits, hits + (slot.request.reverbEnabled ? slot.request.reflectionRayCount * kReverbBounces : 0u));
        }
        result.rayCount = slot.request.rayCount;
        result.seed = slot.request.seed;
        result.sceneGeneration = slot.request.sceneGeneration;
        result.sourceId = slot.request.sourceId;
        result.sequenceId = slot.request.sequenceId;
        result.inputTime = slot.request.inputTime;

        if (m_readyResult) m_statistics.discarded++;
        m_readyResult = result;
        m_latestSequence = result.sequenceId;
    }
}

namespace {
    float distanceGain(float length) {
        //Amplitude drops off as 1/r, capped at 1 m so something right next to the listener doesn't blow up
        return 1.0f / std::max(length, 1.0f);
    }

    Craig::SoundArrival makeArrival(const glm::vec3& direction, float length, const std::array<float, 3>& bandGains,
        const std::array<float, 3>& listenerRight, uint32_t order) {
        Craig::SoundArrival arrival{};
        arrival.delaySeconds = length / Craig::kSpeedOfSound;
        arrival.length = length;
        arrival.order = order;
        //Positive if it's coming from the listener's right
        arrival.pan = std::clamp(glm::dot(direction, glm::vec3(listenerRight[0], listenerRight[1], listenerRight[2])), -1.0f, 1.0f);
        for (uint32_t band = 0; band < Craig::kAcousticBandCount; ++band) {
            arrival.gains[band] = bandGains[band] * distanceGain(length) * std::exp(-Craig::kAirAbsorption[band] * length);
        }
        return arrival;
    }
}

void Craig::SoundPropagation::buildArrivals(const ResultSlot& slot, Result& result) {
    const Request& request = slot.request;
    const auto* blocked = static_cast<const uint32_t*>(slot.results.pMapped);
    std::array<float, 3> transmission{};
    for (uint32_t i = 0; i < request.rayCount; ++i) {
        //Still 0xffffffff means the shader never wrote it
        if (blocked[i] > kAcousticMaterials.size()) throw std::runtime_error("result-slot dispatch did not produce a valid obstruction result");
        if (blocked[i] != 0) result.blocked++;
        for (uint32_t band = 0; band < kAcousticBandCount; ++band) {
            //A blocked sample still lets that wall's transmission through, so a door doesn't go completely silent
            transmission[band] += blocked[i] == 0 ? 1.0f : kAcousticMaterials[blocked[i] - 1].transmission[band];
        }
    }
    for (float& gain : transmission) gain /= static_cast<float>(request.rayCount);

    const glm::vec3 listener(request.listener[0], request.listener[1], request.listener[2]);
    const glm::vec3 source(request.source[0], request.source[1], request.source[2]);
    const float directLength = glm::length(source - listener);
    result.arrivals[0] = makeArrival((source - listener) / directLength, directLength, transmission, request.listenerRight, 0);
    result.arrivalCount = 1;
}

bool Craig::SoundPropagation::poll(Result& result) {
    collectCompleted();
    if (!m_readyResult) return false;

    result = *m_readyResult;
    result.ageMs = std::chrono::duration<double, std::milli>(Clock::now() - result.inputTime).count();
    m_readyResult.reset();
    return true;
}

CraigError Craig::SoundPropagation::runSlotTest(Scene& scene) {
    //Waits for the timeline on purpose, the real thing never does this
    const auto waitForCompletion = [this] {
        const vk::Device device = mp_Device->getLogicalDevice();
        const vk::SemaphoreWaitInfo waitInfo = vk::SemaphoreWaitInfo().setSemaphores(m_VK_completionTimeline).setValues(m_lastSubmissionValue);
        return device.waitSemaphores(waitInfo, 1000000000ull) == vk::Result::eSuccess;
    };

    Request request{};
    request.rayCount = 1;
    request.rays[0] = {{0.0f, 2.0f, -3.0f, 0.001f}, {0.0f, 0.0f, 1.0f, 6.0f}};
    request.sceneGeneration = 1;
    request.sourceId = 1;
    request.inputTime = Clock::now();

    CraigError ret = CRAIG_SUCCESS;
    Result result{};

    //Through the wall like the scene test, should come back blocked
    setContext(1, 1);
    request.sequenceId = 0;
    const bool submitted = trySubmit(scene, request) == SubmitStatus::Submitted;
    const bool gotResult = submitted && waitForCompletion() && poll(result);
    Logger::renderer().info("Ray-query Slot test: submitted {}, got a result {}, blocked {}/{} (expected 1/1) - {}",
        submitted, gotResult, result.blocked, result.rayCount, gotResult && result.blocked == 1 ? "pass" : "FAIL");
    if (!gotResult || result.blocked != 1) ret = CRAIG_FAIL;

    //Submit, then change context before it finishes, the result should never come out of poll
    request.sequenceId = 1;
    const bool oldSubmitted = trySubmit(scene, request) == SubmitStatus::Submitted;
    setContext(2, 1);
    const bool oldCompleted = oldSubmitted && waitForCompletion();
    const bool staleResult = oldCompleted && poll(result);
    //A timeout isn't a successful rejection, the GPU has to finish before this check means anything
    const bool rejected = oldCompleted && !staleResult;
    Logger::renderer().info("Ray-query Slot test: old submission completed {}, stale result {} - {}", oldCompleted, staleResult, rejected ? "pass" : "FAIL");
    if (!rejected) ret = CRAIG_FAIL;

    stop();
    return ret;
}
