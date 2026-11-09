//Offline check against Steam Audio's shade maths, not the full stock CPU simulator or a benchmark
//Build the Debug engine first, then from the repo root:
//   g++ -std=c++20 -O2 -pthread -ICraig_Vulkan/Craig -ICraig_Vulkan/External tools/steam_audio_parity.cpp \
//     cmake-build-debug/CMakeFiles/Craig_Vulkan.dir/Craig_Vulkan/Craig/{Craig_Reverb,Craig_Logger}.cpp.o \
//     cmake-build-debug/libFFTConvolver.a cmake-build-debug/libSteamAudioReconstruction.a -lvulkan -o /tmp/craig-steam-parity
//AS setup follows Sascha Willems' ray query example (MIT) - https://github.com/SaschaWillems/Vulkan
//The Steam Audio CPU formula reference and credit are in steam_audio_reference.hpp
#include <vulkan/vulkan.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include "steam_audio_reference.hpp"
#include "Craig_Reverb.hpp"
#include "Renderer/Craig_SteamAudioSampling.hpp"
#include "SteamAudio/reconstructor.h"

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
static void vkCheck(VkResult result) {
    if (result != VK_SUCCESS) throw std::runtime_error("Vulkan result " + std::to_string(result));
}
using V4 = std::array<float, 4>;

struct GPU {
    struct Buffer {
        VkBuffer handle{};
        VkDeviceMemory memory{};
        void* mapped{};
        VkDeviceAddress address{};
    };
    VkInstance instance{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    VkQueue queue{};
    uint32_t family{};
    VkCommandPool pool{};
    VkCommandBuffer command{};
    std::deque<Buffer> buffers;
    std::vector<VkAccelerationStructureKHR> structures;
    PFN_vkCreateAccelerationStructureKHR createAS{};
    PFN_vkDestroyAccelerationStructureKHR destroyAS{};
    PFN_vkGetAccelerationStructureBuildSizesKHR buildSizes{};
    PFN_vkGetAccelerationStructureDeviceAddressKHR asAddress{};
    PFN_vkCmdBuildAccelerationStructuresKHR buildAS{};
    GPU() {
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
        app.apiVersion = VK_API_VERSION_1_2;
        VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
        info.pApplicationInfo = &app;
        vkCheck(vkCreateInstance(&info, nullptr, &instance));
        uint32_t count = 0;
        vkCheck(vkEnumeratePhysicalDevices(instance, &count, nullptr));
        std::vector<VkPhysicalDevice> devices(count);
        vkCheck(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
        for (auto candidate : devices) {
            VkPhysicalDeviceRayQueryFeaturesKHR rq{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
            VkPhysicalDeviceFeatures2 f{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            f.pNext = &rq;
            vkGetPhysicalDeviceFeatures2(candidate, &f);
            if (rq.rayQuery) {
                physical = candidate;
                break;
            }
        }
        require(physical, "no ray-query GPU for this offline check");
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical, &properties);
        std::cout << "GPU: " << properties.deviceName << '\n';
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical, &count, families.data());
        for (family = 0; family < count; ++family)
            if (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) break;
        require(family < count, "no compute queue");
        const float priority = 1;
        VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        q.queueFamilyIndex = family;
        q.queueCount = 1;
        q.pQueuePriorities = &priority;
        VkPhysicalDeviceRayQueryFeaturesKHR rq{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};
        rq.rayQuery = VK_TRUE;
        VkPhysicalDeviceAccelerationStructureFeaturesKHR af{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
        af.accelerationStructure = VK_TRUE;
        af.pNext = &rq;
        VkPhysicalDeviceVulkan12Features f12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        f12.bufferDeviceAddress = VK_TRUE;
        f12.pNext = &af;
        const char* extensions[] = {VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, VK_KHR_RAY_QUERY_EXTENSION_NAME,
                                    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME};
        VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        dc.pNext = &f12;
        dc.queueCreateInfoCount = 1;
        dc.pQueueCreateInfos = &q;
        dc.enabledExtensionCount = 3;
        dc.ppEnabledExtensionNames = extensions;
        vkCheck(vkCreateDevice(physical, &dc, nullptr, &device));
        vkGetDeviceQueue(device, family, 0, &queue);
#define LOAD(member, name)                                                                                                                           \
    member = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name));                                                                       \
    require(member, #name)
        LOAD(createAS, vkCreateAccelerationStructureKHR);
        LOAD(destroyAS, vkDestroyAccelerationStructureKHR);
        LOAD(buildSizes, vkGetAccelerationStructureBuildSizesKHR);
        LOAD(asAddress, vkGetAccelerationStructureDeviceAddressKHR);
        LOAD(buildAS, vkCmdBuildAccelerationStructuresKHR);
#undef LOAD
        VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pc.queueFamilyIndex = family;
        pc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        vkCheck(vkCreateCommandPool(device, &pc, nullptr, &pool));
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ca.commandPool = pool;
        ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ca.commandBufferCount = 1;
        vkCheck(vkAllocateCommandBuffers(device, &ca, &command));
    }
    ~GPU() {
        if (device) {
            vkDeviceWaitIdle(device);
            for (auto a : structures) destroyAS(device, a, nullptr);
            for (auto& b : buffers) {
                vkUnmapMemory(device, b.memory);
                vkDestroyBuffer(device, b.handle, nullptr);
                vkFreeMemory(device, b.memory, nullptr);
            }
            vkDestroyCommandPool(device, pool, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (instance) vkDestroyInstance(instance, nullptr);
    }
    Buffer& buffer(size_t size, VkBufferUsageFlags usage) {
        buffers.emplace_back();
        auto& b = buffers.back();
        VkBufferCreateInfo bc{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bc.size = size;
        bc.usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        bc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(vkCreateBuffer(device, &bc, nullptr, &b.handle));
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, b.handle, &req);
        VkPhysicalDeviceMemoryProperties mp{};
        vkGetPhysicalDeviceMemoryProperties(physical, &mp);
        uint32_t type = 0;
        for (; type < mp.memoryTypeCount; ++type)
            if ((req.memoryTypeBits & (1u << type)) &&
                (mp.memoryTypes[type].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
                break;
        require(type < mp.memoryTypeCount, "no coherent memory");
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
        flags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.pNext = &flags;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = type;
        vkCheck(vkAllocateMemory(device, &ai, nullptr, &b.memory));
        vkCheck(vkBindBufferMemory(device, b.handle, b.memory, 0));
        vkCheck(vkMapMemory(device, b.memory, 0, VK_WHOLE_SIZE, 0, &b.mapped));
        VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
        address.buffer = b.handle;
        b.address = vkGetBufferDeviceAddress(device, &address);
        return b;
    }
    void begin() {
        vkCheck(vkResetCommandBuffer(command, 0));
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        vkCheck(vkBeginCommandBuffer(command, &bi));
    }
    void end() {
        vkCheck(vkEndCommandBuffer(command));
        VkSubmitInfo s{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        s.commandBufferCount = 1;
        s.pCommandBuffers = &command;
        vkCheck(vkQueueSubmit(queue, 1, &s, VK_NULL_HANDLE));
        vkCheck(vkQueueWaitIdle(queue));
    }
    VkAccelerationStructureKHR build(VkAccelerationStructureTypeKHR type, VkAccelerationStructureGeometryKHR geometry, uint32_t count) {
        VkAccelerationStructureBuildGeometryInfoKHR bi{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
        bi.type = type;
        bi.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        bi.geometryCount = 1;
        bi.pGeometries = &geometry;
        VkAccelerationStructureBuildSizesInfoKHR sizes{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
        buildSizes(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &bi, &count, &sizes);
        auto& storage = buffer(sizes.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR);
        VkAccelerationStructureCreateInfoKHR ac{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
        ac.buffer = storage.handle;
        ac.size = sizes.accelerationStructureSize;
        ac.type = type;
        VkAccelerationStructureKHR handle{};
        vkCheck(createAS(device, &ac, nullptr, &handle));
        structures.push_back(handle);
        bi.dstAccelerationStructure = handle;
        VkPhysicalDeviceAccelerationStructurePropertiesKHR ap{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2 pp{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        pp.pNext = &ap;
        vkGetPhysicalDeviceProperties2(physical, &pp);
        auto align = ap.minAccelerationStructureScratchOffsetAlignment;
        auto& scratch = buffer(std::max<VkDeviceSize>(1, sizes.buildScratchSize) + align, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        bi.scratchData.deviceAddress = (scratch.address + align - 1) & ~VkDeviceAddress(align - 1);
        VkAccelerationStructureBuildRangeInfoKHR range{};
        range.primitiveCount = count;
        const auto* ranges = &range;
        begin();
        buildAS(command, 1, &bi, &ranges);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                             VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0,
                             nullptr, 0, nullptr);
        end();
        return handle;
    }
    VkDeviceAddress address(VkAccelerationStructureKHR handle) {
        VkAccelerationStructureDeviceAddressInfoKHR ai{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
        ai.accelerationStructure = handle;
        return asAddress(device, &ai);
    }
};

static void checkGPU(const std::filesystem::path& shader) {
    GPU gpu;
    const float vertices[] = {-100, 0, -100, 100, 0, -100, 100, 0, 100, -100, 0, -100, 100, 0, 100, -100, 0, 100,
                              -100, 8, -100, 100, 8, -100, 100, 8, 100, -100, 8, -100, 100, 8, 100, -100, 8, 100};
    auto& positions = gpu.buffer(sizeof(vertices), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    std::memcpy(positions.mapped, vertices, sizeof(vertices));
    VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress = positions.address;
    geometry.geometry.triangles.vertexStride = 3 * sizeof(float);
    geometry.geometry.triangles.maxVertex = 11;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
    auto blas = gpu.build(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR, geometry, 4);
    VkAccelerationStructureInstanceKHR instance{};
    instance.transform.matrix[0][0] = instance.transform.matrix[1][1] = instance.transform.matrix[2][2] = 1;
    instance.mask = 0xff;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = gpu.address(blas);
    auto& instances = gpu.buffer(sizeof(instance), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    std::memcpy(instances.mapped, &instance, sizeof(instance));
    geometry = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = {VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    geometry.geometry.instances.data.deviceAddress = instances.address;
    auto tlas = gpu.build(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR, geometry, 1);
    constexpr uint32_t maxRays = 4096;
    std::array<GPU::Buffer*, 8> storage = {&gpu.buffer(32, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                           &gpu.buffer(4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                           &gpu.buffer((maxRays + 1024) * sizeof(V4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                           &gpu.buffer(8, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                           &gpu.buffer(64, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                           &gpu.buffer(32, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                           &gpu.buffer(maxRays * 64 * sizeof(V4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                                           &gpu.buffer(maxRays * 64 * sizeof(V4), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)};
    std::memset(storage[3]->mapped, 0, 8);
    const V4 normals[] = {{0, 1, 0, 0}, {0, 1, 0, 0}, {0, -1, 0, 0}, {0, -1, 0, 0}};
    std::memcpy(storage[4]->mapped, normals, sizeof(normals));
    const glm::vec3 listener(0, 2, 0), source(1, 1, 2), direct = glm::normalize(listener - source);
    const V4 ray[] = {{source.x, source.y, source.z, .001f}, {direct.x, direct.y, direct.z, glm::length(listener - source) - .001f}};
    std::memcpy(storage[0]->mapped, ray, sizeof(ray));
    constexpr uint32_t bindings[] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::array<VkDescriptorSetLayoutBinding, 9> layoutBindings{};
    layoutBindings[0] = {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    for (int i = 0; i < 8; ++i) layoutBindings[i + 1] = {bindings[i], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo lc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lc.bindingCount = 9;
    lc.pBindings = layoutBindings.data();
    VkDescriptorSetLayout layout{};
    vkCheck(vkCreateDescriptorSetLayout(gpu.device, &lc, nullptr, &layout));
    const VkDescriptorPoolSize sizes[] = {{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8}};
    VkDescriptorPoolCreateInfo pc{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pc.maxSets = 1;
    pc.poolSizeCount = 2;
    pc.pPoolSizes = sizes;
    VkDescriptorPool pool{};
    vkCheck(vkCreateDescriptorPool(gpu.device, &pc, nullptr, &pool));
    VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    da.descriptorPool = pool;
    da.descriptorSetCount = 1;
    da.pSetLayouts = &layout;
    VkDescriptorSet set{};
    vkCheck(vkAllocateDescriptorSets(gpu.device, &da, &set));
    VkWriteDescriptorSetAccelerationStructureKHR aw{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    aw.accelerationStructureCount = 1;
    aw.pAccelerationStructures = &tlas;
    std::array<VkWriteDescriptorSet, 9> writes{};
    std::array<VkDescriptorBufferInfo, 8> bufferInfo{};
    writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    writes[0].pNext = &aw;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    for (int i = 0; i < 8; ++i) {
        bufferInfo[i] = {storage[i]->handle, 0, VK_WHOLE_SIZE};
        writes[i + 1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[i + 1].dstSet = set;
        writes[i + 1].dstBinding = bindings[i];
        writes[i + 1].descriptorCount = 1;
        writes[i + 1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i + 1].pBufferInfo = &bufferInfo[i];
    }
    vkUpdateDescriptorSets(gpu.device, 9, writes.data(), 0, nullptr);
    struct Settings {
        V4 listener, source;
        uint32_t directRays, reflectionRays, bounces, seed, capture;
    };
    static_assert(sizeof(Settings) == 52);
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Settings)};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &layout;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &range;
    VkPipelineLayout pipelineLayout{};
    vkCheck(vkCreatePipelineLayout(gpu.device, &pl, nullptr, &pipelineLayout));
    std::ifstream file(shader, std::ios::binary | std::ios::ate);
    require(bool(file), "compiled production shader missing");
    const auto bytes = static_cast<size_t>(file.tellg());
    std::vector<uint32_t> code(bytes / 4);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), bytes);
    VkShaderModuleCreateInfo mc{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    mc.codeSize = bytes;
    mc.pCode = code.data();
    VkShaderModule module{};
    vkCheck(vkCreateShaderModule(gpu.device, &mc, nullptr, &module));
    VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    cp.layout = pipelineLayout;
    cp.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cp.stage.module = module;
    cp.stage.pName = "main";
    VkPipeline pipeline{};
    vkCheck(vkCreateComputePipelines(gpu.device, VK_NULL_HANDLE, 1, &cp, nullptr, &pipeline));
    SteamReference::CoordinateSpace3f rl{SteamReference::Vector3f(listener)}, rs{SteamReference::Vector3f(source)};
    SteamReference::Directivity directivity;
    SteamReference::ReflectionSimulator reference{&rl, &rs, &directivity};
    SteamReference::IScene scene;
    double largestEnergyError = 0, largestTimeError = 0;
    size_t compared = 0;
    for (uint32_t count : {64u, 256u, 1024u, 4096u})
        for (float scattering : {0.0f, .05f, 1.0f})
            for (uint32_t depth : {1u, 64u}) {
                const float material[] = {.05f, .07f, .08f, scattering, 0, 0, 0, 0};
                std::memcpy(storage[5]->mapped, material, sizeof(material));
                auto* table = static_cast<std::array<float, 4>*>(storage[2]->mapped);
                Craig::fillSteamAudioSamples(std::span(table, count), std::span(table + maxRays, 1024));
                std::memset(storage[7]->mapped, 0xff, count * 64 * sizeof(V4));
                Settings settings{{0, 2, 0, 0}, {1, 1, 2, 0}, 1, count, depth, 17, 1};
                gpu.begin();
                vkCmdBindPipeline(gpu.command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
                vkCmdBindDescriptorSets(gpu.command, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
                vkCmdPushConstants(gpu.command, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(settings), &settings);
                vkCmdDispatch(gpu.command, (count + 63) / 64, 1, 1);
                VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
                barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                vkCmdPipelineBarrier(gpu.command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0,
                                     nullptr);
                gpu.end();
                require(*static_cast<uint32_t*>(storage[1]->mapped) == 0, "clear direct ray became blocked");
                const auto* records = static_cast<const V4*>(storage[7]->mapped);
                std::vector<SteamReference::Vector3f> directions(count);
                SteamReference::generateSphereSamples(count, directions.data());
                SteamReference::Material rm{{.05f, .07f, .08f}, scattering};
                for (uint32_t i = 0; i < count; ++i) {
                    std::array<V4, 64> expected{};
                    glm::vec3 origin = listener, dir = directions[i].value;
                    float accumulated = 0;
                    float energy[] = {1, 1, 1};
                    uint32_t rng = (i + 1u) * 747796405u + 17u * 2891336453u;
                    if (!rng) rng = 1;
                    const auto random = [&]() {
                        rng ^= rng << 13u;
                        rng ^= rng >> 17u;
                        rng ^= rng << 5u;
                        return rng;
                    };
                    const auto sphereHit = [](glm::vec3 o, glm::vec3 d, glm::vec3 c) {
                        const auto v = o - c;
                        const float b = 2 * glm::dot(v, d), disc = b * b - 4 * (glm::dot(v, v) - .01f);
                        return disc < 0 ? INFINITY : -.5f * (b + std::sqrt(disc));
                    };
                    for (uint32_t bounce = 0; bounce < depth && accumulated <= 680; ++bounce) {
                        if (dir.y == 0) break;
                        const float t = ((dir.y < 0 ? 0.0f : 8.0f) - origin.y) / dir.y;
                        const glm::vec3 point = origin + t * dir;
                        if (t <= .1f || std::abs(point.x) > 100 || std::abs(point.z) > 100) break;
                        if (bounce) {
                            const float s = sphereHit(origin, dir, source), l = sphereHit(origin, dir, listener);
                            if ((s >= 0 && s < t) || (l >= 0 && l < t)) break;
                        }
                        const glm::vec3 normal(0, dir.y < 0 ? 1.0f : -1.0f, 0), offsetPoint = point + .01f * normal;
                        const SteamReference::Hit hit{t, SteamReference::Vector3f(normal), &rm};
                        float bands[3]{}, delay = 0;
                        if (reference.shade(scene, {SteamReference::Vector3f(origin), SteamReference::Vector3f(dir)}, bounce, 0, hit,
                                            SteamReference::Vector3f(offsetPoint), energy, accumulated, 4 * SteamReference::Math::kPi / count, bands,
                                            delay) &&
                            delay >= 0 && delay < 2) {
                            for (int b = 0; b < 3; ++b) expected[bounce][b] = .28209479177387814f * bands[b];
                            expected[bounce][3] = delay;
                        }
                        for (int b = 0; b < 3; ++b) energy[b] *= 1 - rm.absorption[b];
                        accumulated += t;
                        origin = offsetPoint;
                        if (float(random() >> 8u) / 16777216.0f < scattering) {
                            const uint32_t index = random() % 1024;
                            const float radius = std::sqrt((index + .5f) / 1024),
                                        theta = 2 * SteamReference::Math::kPi * SteamReference::hammersley(index);
                            const float x = radius * std::cos(theta), y = radius * std::sin(theta), z = -std::sqrt(std::max(0.0f, 1 - x * x - y * y));
                            const glm::vec3 right = std::abs(normal.x) > std::abs(normal.z) ? glm::normalize(glm::vec3(-normal.y, normal.x, 0))
                                                                                            : glm::normalize(glm::vec3(0, -normal.z, normal.y));
                            dir = glm::normalize(x * right + y * glm::cross(right, normal) - z * normal);
                        } else
                            dir = glm::reflect(dir, normal);
                    }
                    for (uint32_t bounce = 0; bounce < 64; ++bounce)
                        for (int b = 0; b < 4; ++b) {
                            const double error = std::abs(double(records[i * 64 + bounce][b]) - expected[bounce][b]);
                            require(std::isfinite(records[i * 64 + bounce][b]), "GPU left an unwritten/nonfinite record");
                            if (b < 3) {
                                largestEnergyError = std::max(largestEnergyError, error);
                                require(error < 2e-8 + 2e-4 * std::abs(expected[bounce][b]), "GPU/Steam CPU reflection weight mismatch");
                            } else {
                                largestTimeError = std::max(largestTimeError, error);
                                if (error >= 2e-5) {
                                    std::cerr << "delay mismatch: rays=" << count << " scatter=" << scattering << " depth=" << depth << " ray=" << i
                                              << " bounce=" << bounce << " GPU=" << records[i * 64 + bounce][b] << " CPU=" << expected[bounce][b]
                                              << '\n';
                                }
                                require(error < 2e-5, "GPU/Steam CPU delay mismatch");
                            }
                        }
                    ++compared;
                }
            }
    std::cout << "GPU production shader / Steam CPU shade: " << compared << " rays; peak energy error " << largestEnergyError << ", delay error "
              << largestTimeError << " s\n";
    vkDestroyPipeline(gpu.device, pipeline, nullptr);
    vkDestroyShaderModule(gpu.device, module, nullptr);
    vkDestroyPipelineLayout(gpu.device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(gpu.device, pool, nullptr);
    vkDestroyDescriptorSetLayout(gpu.device, layout, nullptr);
}

static void checkReconstruction() {
    for (int rate : {44100, 48000, 96000}) {
        Craig::ReverbEnergy e{};
        e[0] = {.01f, .02f, .03f};
        e[30] = {.02f, .015f, .01f};
        e[199] = {.004f, .003f, .002f};
        ipl::EnergyField field(2, 0);
        for (int b = 0; b < 3; ++b)
            for (int i = 0; i < 200; ++i) field[0][b][i] = e[i][b];
        ipl::ImpulseResponse ir(2, 0, rate);
        ipl::Reconstructor upstream(2, 0, rate);
        ipl::AirAbsorptionModel air;
        const ipl::EnergyField* fields[] = {&field};
        const float* curves[] = {nullptr};
        ipl::ImpulseResponse* irs[] = {&ir};
        upstream.reconstruct(1, fields, curves, &air, irs, ipl::ReconstructionType::Linear, 2, 0);
        const auto actual = Craig::Reverb::reconstruct(e, rate, .07f);
        const size_t offset = std::lround(.07f * rate);
        require(actual[0].size() == offset + ir.numSamples(), "reflection pre-delay size mismatch");
        for (size_t i = 0; i < actual[0].size(); ++i) {
            const float expected = i < offset ? 0 : ir[0][i - offset] * std::sqrt(.5f);
            require(actual[0][i] == expected && actual[1][i] == expected, "upstream reconstruction changed in adapter");
        }
        require(std::any_of(ir[0], ir[0] + rate / 20, [](float x) { return x != 0; }), "early reflections were cut off");
        //Upstream IIR adds 1e-9 to avoid denormals, so its zero-input IR isn't bitwise zero
        Craig::ReverbEnergy zero{};
        const auto silent = Craig::Reverb::reconstruct(zero, rate);
        float floor = 0;
        for (float sample : silent[0]) floor = std::max(floor, std::abs(sample));
        std::cout << "Upstream zero-field filter floor: " << floor << '\n';
        require(floor < 1e-8f, "zero field exceeded upstream filter bias");
        std::cout << "Native Steam reconstruction + absolute pre-delay: exact at " << rate << " Hz\n";
    }
}

static void checkDrain() {
    for (int rate : {44100, 48000, 96000}) {
        Craig::Reverb r;
        r.start();
        float l = 0, h = 0;
        r.processSample(0, rate, l, h);
        Craig::ReverbEnergy old{};
        old[30] = {.02f, .015f, .01f};
        old[199] = {.004f, .003f, .002f};
        r.submit(old, .3f);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!r.ready()) {
            l = h = 0;
            r.processSample(0, rate, l, h);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            require(std::chrono::steady_clock::now() < deadline, "worker did not prepare response");
        }
        const auto expected = Craig::Reverb::reconstruct(old, rate, .3f);
        const int frames = rate * 3;
        double error = 0;
        for (int i = 0; i < frames; ++i) {
            if (i == rate / 10) {
                auto newer = old;
                newer[30][0] *= 2;
                r.submit(newer, .02f);
            }
            l = h = 0;
            r.processSample(i == 0 ? 1.0f : 0.0f, rate, l, h);
            const float want = i < int(expected[0].size()) ? expected[0][i] : 0;
            error = std::max(error, double(std::abs(l - want)));
            error = std::max(error, double(std::abs(h - want)));
            if (i >= rate / 10 && r.changes() < 2) std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        require(r.changes() == 2, "changed filter never activated");
        require(!r.failed(), "worker failed");
        require(error < 2e-6, "replacement erased or changed previously emitted response");
        r.stop();
        std::cout << "Old response including pre-delay/final bin drains at " << rate << " Hz; peak error " << error << '\n';
    }
}

int main() {
    try {
        const auto shader = std::filesystem::absolute("Craig_Vulkan/data/shaders/SoundPropagation.spv");
        auto temp = std::filesystem::temp_directory_path() /
                    ("craig-steam-check-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(temp / "logs");
        std::filesystem::current_path(temp);
        checkGPU(shader);
        checkReconstruction();
        checkDrain();
        std::cout << "PASS (development conformance checks, not a full stock-library CPU benchmark)\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
