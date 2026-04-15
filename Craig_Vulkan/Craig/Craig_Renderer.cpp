#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

#include <cassert>
#include <iostream>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

#if defined(IMGUI_ENABLED)
#include "../External/Imgui/imgui.h"   
#include "../External/Imgui/imgui_impl_vulkan.h"
#include "../External/Imgui/imgui_impl_sdl2.h"
#endif

#include "Craig_Renderer.hpp"
#include "Craig_Window.hpp"
#include "Craig_ShaderCompilation.hpp"
#include "Craig_Editor.hpp"
#include "Craig_SceneManager.hpp"

#include "Renderer/Craig_Swapchain.hpp"
#include "Renderer/Craig_Device.hpp"
#include "Renderer/Craig_ImageHelpers.hpp"
#include "Renderer/Craig_Instance.hpp"
#include "Renderer/Craig_Pipeline.hpp"
#include "Renderer/Craig_SyncManager.hpp"

#if defined(IMGUI_ENABLED)
static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan-imgui] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}
#endif

CraigError Craig::Renderer::init(Window* CurrentWindowPtr, SceneManager* sceneManagerPtr) {

	CraigError ret = CRAIG_SUCCESS;

	// Check if the current window pointer is valid
	assert(CurrentWindowPtr != nullptr && "CurrentWindowPtr is null, cannot initialize Renderer without a valid window pointer.");
	//Pass in the current window pointer (Done in framework)
	mp_CurrentWindow = CurrentWindowPtr; 

    // Check if the current scene manager pointer is valid
    assert(sceneManagerPtr != nullptr && "sceneManagerPtr is null, cannot initialize Renderer without a valid window pointer.");
    //Pass in the current scene manager pointer (Done in framework)
    mp_SceneManager = sceneManagerPtr;

	// Ensure that the current window pointer is not null (just to be extra safe)
	assert(mp_CurrentWindow != nullptr && "mp_CurrentWindow is null, somehow didn't get passed to our member variable");

	// Use validation layers if this is a debug build
#if defined(_DEBUG)
    mv_VK_Layers.push_back("VK_LAYER_KHRONOS_validation");
    mp_CurrentWindow->getExtensionsVector().push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif


    Instance::InstanceInitInfo instanceInitInfo;
    instanceInitInfo.validationLayerVector = mv_VK_Layers;
    instanceInitInfo.p_Window = mp_CurrentWindow;

    m_instance.init(instanceInitInfo);

    InitVulkan();

#if defined(IMGUI_ENABLED)
    InitImgui();

#endif

	return ret;
}

#if defined(IMGUI_ENABLED)

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
    init_info.Instance = m_instance.getVkInstance();
    init_info.PhysicalDevice = m_Devices.getPhysicalDevice();
    init_info.Device = m_Devices.getLogicalDevice();

    Craig::Device::QueueFamilyIndices indices = Craig::Device::findQueueFamilies(m_Devices.getPhysicalDevice(), m_instance.getVkSurface());
    init_info.QueueFamily = indices.graphicsFamily.value();
    init_info.Queue = m_Devices.getGraphicsQueue();
    init_info.DescriptorPool = m_VK_imguiDescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = 2;
    init_info.ImageCount = kMaxFramesInFlight;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn = check_vk_result;
    init_info.UseDynamicRendering = true;

    // dynamic rendering parameters for imgui to use
    init_info.PipelineRenderingCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    VkFormat colourFormat = static_cast<VkFormat>(m_swapChain.getImageFormat());
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colourFormat;

    ImGui_ImplVulkan_Init(&init_info);

}
#endif

CraigError Craig::Renderer::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

#if defined(IMGUI_ENABLED)
    if (m_swapChain.getExtent().width > 0 && m_swapChain.getExtent().height > 0) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        Craig::ImguiEditor::getInstance().editorMain(deltaTime);
    }

#endif

    drawFrame(deltaTime);

	return ret;
}


void Craig::Renderer::InitVulkan() {

    Device::DeviceInitInfo deviceInitInfo;
    deviceInitInfo.surface = m_instance.getVkSurface();
    deviceInitInfo.instance = m_instance.getVkInstance();
    deviceInitInfo.deviceExtensionsVector = mv_VK_deviceExtensions;

    m_Devices.init(deviceInitInfo); //Picks physical device, creates logical device

    Swapchain::SwapchainInitInfo swapInitInfo;
    swapInitInfo.surface = m_instance.getVkSurface();
    swapInitInfo.device = m_Devices.getLogicalDevice();
    swapInitInfo.physicalDevice = m_Devices.getPhysicalDevice();
    swapInitInfo.pWindow = mp_CurrentWindow;

    m_swapChain.init(swapInitInfo);

    RenderingAttachments::RenderingAttachmentsInitInfo renderingAttachmentsInitInfo;

    renderingAttachmentsInitInfo.surface = m_instance.getVkSurface();
    renderingAttachmentsInitInfo.device = m_Devices.getLogicalDevice();
    renderingAttachmentsInitInfo.physicalDevice = m_Devices.getPhysicalDevice();
    renderingAttachmentsInitInfo.memoryAllocator = m_Devices.getVmaAllocator();

    m_renderingAttachments.init(renderingAttachmentsInitInfo);
    m_renderingAttachments.createColourResources(m_swapChain.getExtent(), m_swapChain.getImageFormat());
    m_renderingAttachments.createDepthResources(m_swapChain.getExtent());

    Pipeline::PipelineInitInfo pipelineInitInfo;
    pipelineInitInfo.device = m_Devices.getLogicalDevice();
    pipelineInitInfo.colorFormat = m_swapChain.getImageFormat();
    pipelineInitInfo.depthFormat = m_renderingAttachments.findDepthFormat();
    pipelineInitInfo.msaaSamples = &m_renderingAttachments.m_VK_msaaSamples;

    m_pipeline.init(pipelineInitInfo);

    CommandManager::CommandManagerInitInfo commandManagerInitInfo;
    commandManagerInitInfo.p_Device = &m_Devices;
    commandManagerInitInfo.surface = m_instance.getVkSurface();

    m_commandManager.init(commandManagerInitInfo);


    mp_SceneManager->init();

    createTextureSampler();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    Craig::SyncManager::SyncManagerInitInfo syncManagerInitInfo;
    syncManagerInitInfo.logicalDevice = m_Devices.getLogicalDevice();
    syncManagerInitInfo.swapChainImageCount = m_swapChain.getImages().size();

    m_syncManager.init(syncManagerInitInfo);


#if defined(IMGUI_ENABLED)
    createImguiDescriptorPool();
#endif

    mp_CurrentWindow->setCameraRef(&m_camera);
    Craig::ImguiEditor::getInstance().setCamera(&m_camera);
}

void Craig::Renderer::recreateSwapChain() {

    m_swapChain.setSwapExtent();

    if (m_swapChain.getExtent().width <= 0 || m_swapChain.getExtent().height <= 0) {
        return; // Skip this frame
    }

    m_Devices.getLogicalDevice().waitIdle();

    m_renderingAttachments.cleanupColourAndDepthImageViews();
    m_swapChain.cleanupSwapChain();

    m_swapChain.createSwapChain();
    m_swapChain.createSwapImageViews();
    m_renderingAttachments.createColourResources(m_swapChain.getExtent(), m_swapChain.getImageFormat());
    m_renderingAttachments.createDepthResources(m_swapChain.getExtent());
}

void Craig::Renderer::recreateSwapChainFull() {

    m_swapChain.setSwapExtent();

    if (m_swapChain.getExtent().width <= 0 || m_swapChain.getExtent().height <= 0) {
        return; // Skip this frame
    }


    m_Devices.getLogicalDevice().waitIdle();

    m_pipeline.recreate();

    m_renderingAttachments.cleanupColourAndDepthImageViews();
    m_swapChain.cleanupSwapChain();

    //recreate with the new sample number
    m_swapChain.createSwapChain();
    m_swapChain.createSwapImageViews();
    m_renderingAttachments.createColourResources(m_swapChain.getExtent(), m_swapChain.getImageFormat());
    m_renderingAttachments.createDepthResources(m_swapChain.getExtent());
}


void Craig::Renderer::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {

    vk::CommandBufferBeginInfo beginInfo{};

    if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    //We have to transition the swap image manually, render passes used to do this implicitly :(
    Craig::ImageHelpers::transitionSwapImage(commandBuffer, m_swapChain.getImages()[imageIndex], vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
    Craig::ImageHelpers::transitionSwapImage(commandBuffer, m_renderingAttachments.getColourImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal); //MSAA colour image too
    Craig::ImageHelpers::transitionSwapImage(commandBuffer, m_renderingAttachments.getDepthImage(), vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);


    vk::ClearValue clearColour;
    clearColour.setColor({ kClearColour[0], kClearColour[1], kClearColour[2], kClearColour[3] });

    vk::ClearValue clearDepth;
    clearDepth.setDepthStencil({ 1.0f, 0 });

    // Dynamic rendering attachments for colour and depth
    vk::RenderingAttachmentInfo colourAtt{};
    colourAtt
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eStore)
        .setClearValue(clearColour);

    vk::RenderingAttachmentInfo depthAtt{};
    depthAtt
        .setImageView(m_renderingAttachments.getDepthImageView())
        .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eClear)
        .setStoreOp(vk::AttachmentStoreOp::eDontCare)
        .setClearValue(clearDepth);

    bool msaa = (m_renderingAttachments.m_VK_msaaSamples != vk::SampleCountFlagBits::e1);

    if (!msaa) {
        colourAtt
            .setImageView(m_swapChain.getImageViews()[imageIndex])
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal);
    }
    else {
        colourAtt
            .setImageView(m_renderingAttachments.getColourImageView())
            .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setResolveImageView(m_swapChain.getImageViews()[imageIndex])
            .setResolveImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
            .setResolveMode(vk::ResolveModeFlagBits::eAverage);
    }

    // vk::RenderingInfo begins a dynamic rendering instance.
    vk::RenderingInfo ri{};
    ri
        .setRenderArea({ {0,0}, m_swapChain.getExtent() })
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&colourAtt)
        .setPDepthAttachment(&depthAtt);

    commandBuffer.beginRendering(ri);

    //Binding the vertex buffer
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getGraphicsPipeline());
    vk::Buffer vertexBuffers[] = { m_VK_vertexBuffer };
    vk::DeviceSize offsets[] = { 0 };
    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(m_VK_indexBuffer, 0, vk::IndexType::eUint32);

    // Set the dynamic viewport (covers the whole framebuffer)
    vk::Viewport viewport;
    viewport.setX(0.0f)
        .setY(0.0f)
        .setWidth(static_cast<float>(m_swapChain.getExtent().width))
        .setHeight(static_cast<float>(m_swapChain.getExtent().height))
        .setMinDepth(0.0f)
        .setMaxDepth(1.0f);

    commandBuffer.setViewport(0, viewport);

    // Set the dynamic scissor (no cropping � covers entire area)
    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 })
        .setExtent(m_swapChain.getExtent());

    commandBuffer.setScissor(0, scissor);

    /*
    indexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
    instanceCount: Used for instanced rendering, use 1 if you're not doing that.
    firstIndex: Used as an offset into the index buffer
    vertexOffset: used as an offset into the vertex buffer?
    firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
    */

    std::vector<Craig::GameObject*>& currentSceneObjects = mp_SceneManager->getCurrentScene()->getGameObjects();
    Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();

    for (Craig::GameObject* gameObject : currentSceneObjects)
    {
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline.getPipelineLayout(), 0, gameObject->getDescriptorSets()[m_syncManager.getCurrentFrame()], nullptr);

        Craig::Model& model = resources.getModel(gameObject->getModelPath());
        for (size_t i = 0; i < model.subMeshesCount; i++)
        {
            Craig::SubMesh* submesh = model.subMeshes[i];

            commandBuffer.drawIndexed(
                submesh->indexCount,
                1,
                submesh->indexOffset,
                submesh->vertexOffset,
                0);
        }
    }

    commandBuffer.endRendering();

#if defined(IMGUI_ENABLED)
    //gotta render imgui's UI separately
    vk::RenderingAttachmentInfo uiColourAtt{};
    uiColourAtt
        .setImageView(m_swapChain.getImageViews()[imageIndex])
        .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
        .setLoadOp(vk::AttachmentLoadOp::eLoad)     // keep what scene wrote
        .setStoreOp(vk::AttachmentStoreOp::eStore);

    vk::RenderingInfo uiRi{};
    uiRi
        .setRenderArea({ {0,0}, m_swapChain.getExtent() })
        .setLayerCount(1)
        .setColorAttachmentCount(1)
        .setPColorAttachments(&uiColourAtt)
        .setPDepthAttachment(nullptr);
    commandBuffer.beginRendering(uiRi);

    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    commandBuffer.endRendering();
#endif

    Craig::ImageHelpers::transitionSwapImage(commandBuffer, m_swapChain.getImages()[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

    try {
        commandBuffer.end();
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to record command buffer!");
    }


}

void Craig::Renderer::createVertexBuffer() {

    std::vector<Craig::GameObject*>& currentSceneObjects = mp_SceneManager->getCurrentScene()->getGameObjects();
    Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();

    // Pass 1: assign a global vertexOffset to every submesh across every model,
    // so the single shared vertex buffer holds all geometry in sequence.
    uint32_t totalVertexCount = 0;
    for (Craig::GameObject* gameObject : currentSceneObjects)
    {
        Craig::Model& model = resources.getModel(gameObject->getModelPath());
        for (size_t i = 0; i < model.subMeshesCount; i++)
        {
            Craig::SubMesh* submesh = model.subMeshes[i];
            submesh->vertexOffset = totalVertexCount;
            totalVertexCount += static_cast<uint32_t>(submesh->m_vertices.size());
        }
    }

    if (totalVertexCount == 0) {
        return;
    }

    vk::DeviceSize bufferSize = sizeof(Craig::Vertex) * totalVertexCount;

    vk::Buffer stagingBuffer{};
    VmaAllocation stagingAlloc{};

    VmaAllocationCreateInfo stagingAci{};
    stagingAci.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    m_Devices.createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, stagingAci, stagingBuffer, stagingAlloc);

    void* data;
    vmaMapMemory(m_Devices.getVmaAllocator(), stagingAlloc, &data);

    auto* dst = static_cast<Craig::Vertex*>(data);

    // Pass 2: copy each submesh's vertices into the big staging buffer at the
    // offset we assigned in pass 1. Track which models we've already copied so
    // shared models don't get written twice.
    std::unordered_set<std::string> copiedModels;
    for (Craig::GameObject* gameObject : currentSceneObjects)
    {
        const std::string& path = gameObject->getModelPath();
        if (!copiedModels.insert(path).second) continue;

        Craig::Model& model = resources.getModel(path);
        for (size_t i = 0; i < model.subMeshesCount; ++i) {
            Craig::SubMesh* submesh = model.subMeshes[i];
            std::vector<Craig::Vertex>& verts = submesh->m_vertices;
            if (verts.empty()) continue;

            std::memcpy(dst + submesh->vertexOffset,
                verts.data(),
                sizeof(Craig::Vertex) * verts.size());
        }
    }

    vmaFlushAllocation(m_Devices.getVmaAllocator(), stagingAlloc, 0, bufferSize);
    vmaUnmapMemory(m_Devices.getVmaAllocator(), stagingAlloc);

    VmaAllocationCreateInfo gpuAci{};
    gpuAci.usage = VMA_MEMORY_USAGE_AUTO;
    gpuAci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    m_Devices.createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, gpuAci, m_VK_vertexBuffer, m_VMA_vertexAllocation);

    m_commandManager.copyBuffer(stagingBuffer, m_VK_vertexBuffer, bufferSize);
    vmaDestroyBuffer(m_Devices.getVmaAllocator(), stagingBuffer, stagingAlloc);
}

void Craig::Renderer::createIndexBuffer() {

    std::vector<Craig::GameObject*>& currentSceneObjects = mp_SceneManager->getCurrentScene()->getGameObjects();
    Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();

    // Pass 1: assign a global indexOffset to every submesh across every model.
    uint32_t totalIndexCount = 0;
    for (Craig::GameObject* gameObject : currentSceneObjects)
    {
        Craig::Model& model = resources.getModel(gameObject->getModelPath());
        for (size_t i = 0; i < model.subMeshesCount; ++i) {
            Craig::SubMesh* submesh = model.subMeshes[i];
            submesh->indexOffset = totalIndexCount;
            totalIndexCount += static_cast<uint32_t>(submesh->m_indices.size());
        }
    }

    if (totalIndexCount == 0) {
        return;
    }

    vk::DeviceSize bufferSize = sizeof(uint32_t) * totalIndexCount;

    vk::Buffer stagingBuffer{};
    VmaAllocation stagingAlloc{};

    VmaAllocationCreateInfo stagingAci{};
    stagingAci.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    m_Devices.createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, stagingAci, stagingBuffer, stagingAlloc);

    void* data;
    vmaMapMemory(m_Devices.getVmaAllocator(), stagingAlloc, &data);

    auto* dst = static_cast<uint32_t*>(data);

    // Pass 2: copy each submesh's indices into the staging buffer. Indices are
    // submesh-local; drawIndexed's vertexOffset parameter applies the global
    // vertex offset at draw time.
    std::unordered_set<std::string> copiedModels;
    for (Craig::GameObject* gameObject : currentSceneObjects)
    {
        const std::string& path = gameObject->getModelPath();
        if (!copiedModels.insert(path).second) continue;

        Craig::Model& model = resources.getModel(path);
        for (size_t i = 0; i < model.subMeshesCount; ++i) {
            Craig::SubMesh* submesh = model.subMeshes[i];
            std::vector<uint32_t>& indices = submesh->m_indices;
            if (indices.empty()) continue;

            std::memcpy(dst + submesh->indexOffset,
                indices.data(),
                sizeof(uint32_t) * indices.size());
        }
    }

    vmaFlushAllocation(m_Devices.getVmaAllocator(), stagingAlloc, 0, bufferSize);
    vmaUnmapMemory(m_Devices.getVmaAllocator(), stagingAlloc);

    VmaAllocationCreateInfo gpuAci{};
    gpuAci.usage = VMA_MEMORY_USAGE_AUTO;
    gpuAci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    m_Devices.createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, gpuAci, m_VK_indexBuffer, m_VMA_indexAllocation);

    m_commandManager.copyBuffer(stagingBuffer, m_VK_indexBuffer, bufferSize);
    vmaDestroyBuffer(m_Devices.getVmaAllocator(), stagingBuffer, stagingAlloc);
}

void Craig::Renderer::createUniformBuffers() {
    std::vector<Craig::GameObject*>& currentSceneObjects = mp_SceneManager->getCurrentScene()->getGameObjects();
    size_t numObjects = currentSceneObjects.size();

    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

    vk::Buffer stagingBuffer{};
    VmaAllocation stagingAlloc{};

    VmaAllocationCreateInfo stagingAci{};
    stagingAci.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    stagingAci.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    mv_VK_uniformBuffers.resize(kMaxFramesInFlight * numObjects);
    mv_VK_uniformBuffersAllocations.resize(kMaxFramesInFlight * numObjects);
    mv_VK_uniformBuffersMapped.resize(kMaxFramesInFlight * numObjects);

    for (size_t i = 0; i < kMaxFramesInFlight * numObjects; i++)
    {
        VmaAllocationInfo info{};
        m_Devices.createBufferVMA(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, stagingAci, mv_VK_uniformBuffers[i], mv_VK_uniformBuffersAllocations[i], &info);

        mv_VK_uniformBuffersMapped[i] = info.pMappedData;

    }


}

void Craig::Renderer::createDescriptorPool() {
    std::vector<Craig::GameObject*>& currentSceneObjects = mp_SceneManager->getCurrentScene()->getGameObjects();

    std::array<vk::DescriptorPoolSize, 2> poolSizes;
    poolSizes[0]
        .setType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(static_cast<uint32_t>(kMaxFramesInFlight * currentSceneObjects.size()));
    poolSizes[1]
        .setType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(static_cast<uint32_t>(kMaxFramesInFlight * currentSceneObjects.size()));

    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo
        .setPoolSizeCount(static_cast<uint32_t>(poolSizes.size()))
        .setPoolSizes(poolSizes)
        .setMaxSets(static_cast<uint32_t>(kMaxFramesInFlight * currentSceneObjects.size()));

    m_VK_descriptorPool = m_Devices.getLogicalDevice().createDescriptorPool(poolInfo);

}

void Craig::Renderer::createDescriptorSets() {
    std::vector<Craig::GameObject*>& currentSceneObjects = mp_SceneManager->getCurrentScene()->getGameObjects();
    Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();

    size_t numObjects = currentSceneObjects.size();
    size_t totalSets = kMaxFramesInFlight * numObjects;

    std::vector<vk::DescriptorSetLayout> layouts(totalSets, m_pipeline.getDescriptorSetLayout());
    vk::DescriptorSetAllocateInfo allocInfo{};
    allocInfo.setDescriptorPool(m_VK_descriptorPool)
        .setDescriptorSetCount(static_cast<uint32_t>(totalSets))
        .setSetLayouts(layouts);

    std::vector<vk::DescriptorSet> allSets = m_Devices.getLogicalDevice().allocateDescriptorSets(allocInfo);

    for (size_t frame = 0; frame < kMaxFramesInFlight; frame++)
    {
        for (size_t object = 0; object < numObjects; object++)
        {
            size_t setIndex = frame * numObjects + object;

            Craig::GameObject* gameObject = currentSceneObjects[object];
            gameObject->getDescriptorSets()[frame] = allSets[setIndex];

            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo.setBuffer(mv_VK_uniformBuffers[setIndex])
                .setOffset(0)
                .setRange(sizeof(UniformBufferObject));

            vk::DescriptorImageInfo imageInfo{};
            imageInfo
                .setImageView(resources.getModel(gameObject->getModelPath()).m_texture.m_VK_textureImageView)
                .setSampler(m_VK_textureSampler)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            std::array<vk::WriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0]
                .setDstSet(allSets[setIndex])
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                .setDescriptorCount(1)
                .setBufferInfo(bufferInfo);

            descriptorWrites[1]
                .setDstSet(allSets[setIndex])
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(imageInfo);

            m_Devices.getLogicalDevice().updateDescriptorSets(descriptorWrites, nullptr);

        }
    }

}

void Craig::Renderer::updateDescriptorSets() {

    for (size_t i = 0; i < kMaxFramesInFlight; i++) {

        std::vector<Craig::GameObject*>& currentSceneObjects = mp_SceneManager->getCurrentScene()->getGameObjects();
        Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();

        vk::DescriptorImageInfo imageInfo{};

        for (Craig::GameObject* gameObject : currentSceneObjects)
        {
            imageInfo
                .setImageView(resources.getModel(gameObject->getModelPath()).m_texture.m_VK_textureImageView)
                .setSampler(m_VK_textureSampler)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            vk::WriteDescriptorSet descriptorWrite{};
            descriptorWrite
                .setDstSet(gameObject->getDescriptorSets()[i])
                .setDstBinding(1)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setDescriptorCount(1)
                .setImageInfo(imageInfo);

            m_Devices.getLogicalDevice().updateDescriptorSets(descriptorWrite, nullptr);
        }

    }

}

//TODO: Separate camera
void Craig::Renderer::updateUniformBuffer(uint32_t currentImage, const float& deltaTime) {

    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    m_camera.m_aspect = m_swapChain.getExtent().width / (float)m_swapChain.getExtent().height;


    std::vector<Craig::GameObject*>& currentSceneObjects = mp_SceneManager->getCurrentScene()->getGameObjects();
    Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();

    size_t numObjects = currentSceneObjects.size();

    m_camera.update(deltaTime);

    int i = 0;
    for (Craig::GameObject* gameObject : currentSceneObjects)
    {
        UniformBufferObject ubo{};
        ubo.model = gameObject->GetModelMatrix();
        ubo.view = m_camera.getView();
        ubo.proj = m_camera.getProj();
        size_t bufferIndex = currentImage * numObjects + i;


        memcpy(mv_VK_uniformBuffersMapped[bufferIndex], &ubo, sizeof(ubo));

        i++;
    }





}

void Craig::Renderer::createTextureImage2(const uint8_t* pixels, int texWidth, int texHeight, int texChannels, Craig::Texture* outTexture) { // VmaAllocation* textureMemoryAlloc, vk::Image* outTextureImage, vk::ImageView* outTextureImageView) {
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    outTexture->m_VK_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    vk::Buffer stagingBuffer;
    VmaAllocation stagingAlloc{};

    VmaAllocationCreateInfo stagingAci{};
    stagingAci.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    m_Devices.createBufferVMA(imageSize, vk::BufferUsageFlagBits::eTransferSrc, stagingAci, stagingBuffer, stagingAlloc);

    void* data;
    vmaMapMemory(m_Devices.getVmaAllocator(), stagingAlloc, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vmaFlushAllocation(m_Devices.getVmaAllocator(), stagingAlloc, 0, imageSize);
    vmaUnmapMemory(m_Devices.getVmaAllocator(), stagingAlloc);

    //stbi_image_free(pixels);

    outTexture->m_VK_textureImage = ImageHelpers::createImage(m_Devices.getPhysicalDevice(), m_instance.getVkSurface(), texWidth, texHeight, outTexture->m_VK_mipLevels, vk::SampleCountFlagBits::e1, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, m_Devices.getVmaAllocator(),outTexture->m_VMA_textureImageAllocation);

    Craig::ImageHelpers::transitionImageLayout(m_commandManager ,outTexture->m_VK_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, true, outTexture->m_VK_mipLevels);
    Craig::ImageHelpers::copyBufferToImage(m_commandManager, stagingBuffer, outTexture->m_VK_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    //transitionImageLayout(m_VK_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, false, m_VK_mipLevels); <- now done when generating mipMaps

    vmaDestroyBuffer(m_Devices.getVmaAllocator(), stagingBuffer, stagingAlloc);

    Craig::ImageHelpers::generateMipMaps(m_commandManager, m_Devices.getPhysicalDevice().getFormatProperties(vk::Format::eR8G8B8A8Srgb), outTexture->m_VK_textureImage, texWidth, texHeight, outTexture->m_VK_mipLevels, false);

    outTexture->m_VK_textureImageView = Craig::ImageHelpers::createImageView(m_Devices.getLogicalDevice(), outTexture->m_VK_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor, outTexture->m_VK_mipLevels);

}

void Craig::Renderer::createTextureSampler() {

    vk::PhysicalDeviceProperties physicalDeviceProperties{};
    physicalDeviceProperties = m_Devices.getPhysicalDevice().getProperties();

    vk::SamplerCreateInfo samplerInfo;

    samplerInfo
        //How to interpolate texels that are magnified or minified
        .setMagFilter(vk::Filter::eLinear)
        .setMinFilter(vk::Filter::eLinear)
        //What to do when we try to read texels outside the image
        .setAddressModeU(vk::SamplerAddressMode::eRepeat)
        .setAddressModeV(vk::SamplerAddressMode::eRepeat)
        .setAddressModeW(vk::SamplerAddressMode::eRepeat)
        //Enable anisotropic filtering
        .setAnisotropyEnable(vk::True)
        .setMaxAnisotropy(physicalDeviceProperties.limits.maxSamplerAnisotropy) //We can set it to whatever the max the gpu supports
        //Specify colour returned when sampling beyond the image with clamp to border mode
        .setBorderColor(vk::BorderColor::eFloatOpaqueBlack)
        //Clamp coordinates to 0-1 or 0-texSize
        .setUnnormalizedCoordinates(vk::False)
        //Something about comparing the texels
        .setCompareEnable(vk::False)
        .setCompareOp(vk::CompareOp::eAlways)
        //Mimapping
        .setMipmapMode(vk::SamplerMipmapMode::eLinear)
        .setMinLod(m_minLODLevel)
        .setMaxLod(vk::LodClampNone)
        .setMipLodBias(0.0f);

    m_VK_textureSampler = m_Devices.getLogicalDevice().createSampler(samplerInfo);



}

void Craig::Renderer::updateSamplingLevel(int levelToSet) {


    switch (levelToSet)
    {
    case(64):
        m_renderingAttachments.m_VK_msaaSamples = vk::SampleCountFlagBits::e64;
        break;
    case(32):
        m_renderingAttachments.m_VK_msaaSamples = vk::SampleCountFlagBits::e32;
        break;
    case(16):
        m_renderingAttachments.m_VK_msaaSamples = vk::SampleCountFlagBits::e16;
        break;
    case(8):
        m_renderingAttachments.m_VK_msaaSamples = vk::SampleCountFlagBits::e8;
        break;
    case(4):
        m_renderingAttachments.m_VK_msaaSamples = vk::SampleCountFlagBits::e4;
        break;
    case(2):
        m_renderingAttachments.m_VK_msaaSamples = vk::SampleCountFlagBits::e2;
        break;
    case(1):
        m_renderingAttachments.m_VK_msaaSamples = vk::SampleCountFlagBits::e1;
        break;

    default:
        break;
    }
    recreateSwapChainFull();

}

void Craig::Renderer::updateMinLOD(int minLOD) {
    m_minLODLevel = minLOD;

    terminateSampler();
    createTextureSampler();
    updateDescriptorSets();

    printf("Recreated sampler and updated the descriptor sets to change the LOD \n");
}

void Craig::Renderer::drawFrame(const float& deltaTime) {

    m_syncManager.waitForGpu();
    const uint32_t& currentFrame = m_syncManager.getCurrentFrame();

    if (m_swapChain.getExtent().width <= 0 || m_swapChain.getExtent().height <= 0) {
        return; // Skip this frame
    }

    uint32_t imageIndex = 0;
    VkResult nextImageResult = vkAcquireNextImageKHR(m_Devices.getLogicalDevice(), m_swapChain.getSwapChain(), UINT64_MAX, m_syncManager.getVK_imageAvailableSemaphores()[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (nextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    else if (nextImageResult != VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // Record drawing commands into the command buffer
    m_commandManager.getCommandBuffers()[currentFrame].reset();
    recordCommandBuffer(m_commandManager.getCommandBuffers()[currentFrame], imageIndex);

    updateUniformBuffer(currentFrame, deltaTime);

    //Creates the submit info and submits the command buffer to the gfx queue
    m_syncManager.submitFrame(m_commandManager.getCommandBuffers(), imageIndex, m_Devices.getGraphicsQueue());

    // Present the rendered image to the screen
    vk::PresentInfoKHR presentInfo;
    presentInfo
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&m_syncManager.getVK_renderFinishedSemaphores()[imageIndex])
        .setSwapchainCount(1)
        .setPSwapchains(&m_swapChain.getSwapChain())
        .setPImageIndices(&imageIndex);


    //We have to revert back to the original C code otherwise if it returns ERROR_OUT_OF_DATE, it throws an exception and messes up the code.
    auto presentResult = vkQueuePresentKHR(m_Devices.getPresentationQueue(), presentInfo);

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || mp_CurrentWindow->isResizeNeeded()) {
        recreateSwapChain();
        mp_CurrentWindow->finishedResize();
    }
    else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_syncManager.nextFrame();

}


#if defined(IMGUI_ENABLED)
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

    vk::Result result = m_Devices.getLogicalDevice().createDescriptorPool(&poolInfo, nullptr, &m_VK_imguiDescriptorPool);
    check_vk_result(static_cast<VkResult>(result));
}
#endif

CraigError Craig::Renderer::terminate() {

    CraigError ret = CRAIG_SUCCESS;

    m_Devices.getLogicalDevice().waitIdle();

#if defined(IMGUI_ENABLED)
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    m_Devices.getLogicalDevice().destroyDescriptorPool(m_VK_imguiDescriptorPool);
#endif
    vmaDestroyBuffer(m_Devices.getVmaAllocator(), m_VK_indexBuffer, m_VMA_indexAllocation);

    vmaDestroyBuffer(m_Devices.getVmaAllocator(), m_VK_vertexBuffer, m_VMA_vertexAllocation);

    m_syncManager.terminate();

    m_commandManager.terminate();

    m_renderingAttachments.terminate();

    m_Devices.getLogicalDevice().destroySampler(m_VK_textureSampler);

    Craig::ResourceManager::getInstance().terminateModels(m_Devices.getLogicalDevice(), m_Devices.getVmaAllocator());



    m_swapChain.terminate();

    for (size_t i = 0; i < mv_VK_uniformBuffers.size(); i++) {
        vmaDestroyBuffer(m_Devices.getVmaAllocator(), mv_VK_uniformBuffers[i], mv_VK_uniformBuffersAllocations[i]);
    }

    m_Devices.getLogicalDevice().destroyDescriptorPool(m_VK_descriptorPool);

    m_pipeline.terminate();


    m_Devices.terminate();

    m_instance.terminate();

    return ret;
}

void Craig::Renderer::terminateSampler() {
    m_Devices.getLogicalDevice().waitIdle();
    m_Devices.getLogicalDevice().destroySampler(m_VK_textureSampler);
}
