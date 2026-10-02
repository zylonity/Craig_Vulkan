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
#include "../External/Imgui/imgui_impl_sdl3.h"
#include "../External/Imgui/ImGuizmo/ImGuizmo.h"
#endif

#include "Craig_Renderer.hpp"

#include <sys/stat.h>

#include "Craig_Window.hpp"
#include "Craig_ShaderCompilation.hpp"
#include "Craig_Editor.hpp"
#include "Craig_SceneManager.hpp"
#include "Craig_Profiler.hpp"
#include "Craig_Logger.hpp"
#include "Components/Craig_Model.hpp"
#include "Components/Craig_Sun.hpp"

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
    Craig::Logger::renderer().error("[vulkan-imgui] Error: VkResult = {}", static_cast<int>(err));
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
    ImGui_ImplSDL3_InitForVulkan(mp_CurrentWindow->getSDLWindow());
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_instance.getVkInstance();
    init_info.PhysicalDevice = m_Devices.getPhysicalDevice();
    init_info.Device = m_Devices.getLogicalDevice();
    init_info.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;

    Craig::Device::QueueFamilyIndices indices = Craig::Device::findQueueFamilies(m_Devices.getPhysicalDevice(), m_instance.getVkSurface());
    init_info.QueueFamily = indices.graphicsFamily.value();
    init_info.Queue = m_Devices.getGraphicsQueue();
    init_info.MinImageCount = 2;
    init_info.ImageCount = static_cast<uint32_t>(m_swapChain.getImages().size());
    init_info.CheckVkResultFn = check_vk_result;
    init_info.UseDynamicRendering = true;

    // PipelineInfoMain is left empty so Init doesn't make the pipeline, createImGuiPipeline does it
    ImGui_ImplVulkan_Init(&init_info);
    createImGuiPipeline();

    Craig::Logger::renderer().info("ImGui {} up (SDL3 + Vulkan backends, docking on)", ImGui::GetVersion());
}

// imgui draws in the scene's pass now, so its pipeline needs the same formats + MSAA as that pass
void Craig::Renderer::createImGuiPipeline() {

    ImGui_ImplVulkan_PipelineInfo pipelineInfo{};
    pipelineInfo.PipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    pipelineInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
    VkFormat colourFormat = static_cast<VkFormat>(m_swapChain.getImageFormat());
    pipelineInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &colourFormat; // ImGui makes a copy of this
    pipelineInfo.PipelineRenderingCreateInfo.depthAttachmentFormat = static_cast<VkFormat>(m_renderingAttachments.findDepthFormat());
    pipelineInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(m_renderingAttachments.m_VK_msaaSamples);

    ImGui_ImplVulkan_CreateMainPipeline(&pipelineInfo);
    Craig::Logger::renderer().debug("ImGui pipeline made with {} MSAA", vk::to_string(m_renderingAttachments.m_VK_msaaSamples));
}
#endif

CraigError Craig::Renderer::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

#if defined(IMGUI_ENABLED)
    if (m_swapChain.getExtent().width > 0 && m_swapChain.getExtent().height > 0) {
        CRAIG_PROFILE_SCOPE("  ImGui editor");
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();
        Craig::ImguiEditor::getInstance().editorMain(deltaTime);
    }

#endif

    // a model got added/changed/removed this frame, so the buffers are out of date
    if (mp_SceneManager->getCurrentScene()->consumeGeometryDirty())
    {
        rebuildGeometryBuffers();
        createModelDescriptorSets();
    }

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

    Craig::SyncManager::SyncManagerInitInfo syncManagerInitInfo;
    syncManagerInitInfo.logicalDevice = m_Devices.getLogicalDevice();
    syncManagerInitInfo.swapChainImageCount = m_swapChain.getImages().size();

    m_syncManager.init(syncManagerInitInfo);

}

// Second half of the renderer's init. The first scene loads in between (in the framework) since loading models
// needs the device + command pool to upload textures, and these need the scene's models to build their buffers.
CraigError Craig::Renderer::initSceneResources() {

    CraigError ret = CRAIG_SUCCESS;

    assert(mp_SceneManager->getCurrentScene() != nullptr && "The scene manager has to be initialised before the renderer's scene resources");

    createTextureSampler();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    mp_CurrentWindow->setCameraRef(&mp_SceneManager->getCurrentScene()->getCamera());

#if defined(IMGUI_ENABLED)
    Craig::ImguiEditor::getInstance().setCamera(&mp_SceneManager->getCurrentScene()->getCamera());
#endif

    Craig::Logger::renderer().info("Scene resources ready (sampler, buffers, UBOs, descriptor sets)");

    return ret;
}

void Craig::Renderer::recreateSwapChain() {

    m_swapChain.setSwapExtent();

    if (m_swapChain.getExtent().width <= 0 || m_swapChain.getExtent().height <= 0) {
        Craig::Logger::renderer().trace("Window's 0 size (minimised?), not recreating the swapchain yet");
        return; // Skip this frame
    }

    Craig::Logger::renderer().debug("Recreating the swapchain (resized or out of date)");

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
        Craig::Logger::renderer().trace("Window's 0 size (minimised?), not recreating the swapchain yet");
        return; // Skip this frame
    }

    Craig::Logger::renderer().debug("Recreating the swapchain and pipelines");

    m_Devices.getLogicalDevice().waitIdle();

    m_pipeline.recreate();
#if defined(IMGUI_ENABLED)
    createImGuiPipeline(); // MSAA sample count might have changed
#endif

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
        Craig::Logger::renderer().critical("Failed to begin recording the command buffer");
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
            .setResolveMode(vk::ResolveModeFlagBits::eAverage)
            // We only need the resolved image, writing every MSAA sample out to memory is a waste (especially on Apple's GPUs)
            .setStoreOp(vk::AttachmentStoreOp::eDontCare);
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
    // Buffers are null if nothing in the scene has a model, binding a null buffer is invalid (nothing gets drawn anyway)
    if (m_VK_vertexBuffer && m_VK_indexBuffer)
    {
        commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
        commandBuffer.bindIndexBuffer(m_VK_indexBuffer, 0, vk::IndexType::eUint32);
    }

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

    // Per-frame set (camera UBO + transforms SSBO) only needs binding once per frame, it stays bound for every draw after.
    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        m_pipeline.getPipelineLayout(),
        0, // set 0
        mv_VK_perFrameDescriptorSet[m_syncManager.getCurrentFrame()],
        nullptr);

    for (size_t objectIdx = 0; objectIdx < currentSceneObjects.size(); objectIdx++)
    {
        // objects without a model component (e.g. just a sun) have nothing to draw
        const Craig::Components::Model* pModelComponent = currentSceneObjects[objectIdx]->getComponent<Craig::Components::Model>();
        if (pModelComponent == nullptr || !pModelComponent->hasModel())
        {
            continue;
        }

        // objectIdx still lines up with the SSBO, every object gets a slot whether it draws or not
        Craig::Model& model = resources.getModel(pModelComponent->getModelPath());

        // draw the model's node tree, children get drawn recursively
        for (const Craig::Node* node : model.nodes)
        {
            drawNode(commandBuffer, model, node, static_cast<uint32_t>(objectIdx));
        }
    }

    // Sky goes after the models so the depth test skips every pixel they already cover, the shader's expensive
    // Same pipeline layout so set 0 is still bound, and 3 verts with no buffer is the fullscreen triangle
    if (m_skyEnabled) {
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.getSkyPipeline());
        commandBuffer.draw(3, 1, 0, 0);
    }

#if defined(IMGUI_ENABLED)
    // ImGui used to have its own pass after this one, but with vsync off macOS can show the image in between the two passes
    // (the scene without the UI, looked like horizontal cuts across the imgui windows). Same pass = the image only gets written once
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
#endif

    commandBuffer.endRendering();

    Craig::ImageHelpers::transitionSwapImage(commandBuffer, m_swapChain.getImages()[imageIndex], vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

    try {
        commandBuffer.end();
    }
    catch (const vk::SystemError& err) {
        Craig::Logger::renderer().critical("Failed to record the command buffer: {}", err.what());
        throw std::runtime_error("failed to record command buffer!");
    }


}

// Adapted from drawNode in Sascha Willems' gltfloading example (MIT), see Craig_ResourceManager.cpp
void Craig::Renderer::drawNode(vk::CommandBuffer commandBuffer, Craig::Model& model, const Craig::Node* node, uint32_t objectIndex) {

    if (!node->subMeshes.empty())
    {
        Craig::PushConstantData pushData{};
        pushData.nodeMatrix = node->getWorldMatrix();
        pushData.objectIndex = objectIndex; // Which slot of the SSBO has this object's model matrix

        for (const Craig::SubMesh* submesh : node->subMeshes)
        {
            if (submesh->indexCount == 0) continue;

            // Each primitive can have its own material, so push its colour + bind its textures
            const Craig::Material& material = model.getMaterial(submesh->materialIndex);
            pushData.baseColorFactor = material.baseColorFactor;
            pushData.metallicFactor = material.metallicFactor;
            pushData.roughnessFactor = material.roughnessFactor;

            commandBuffer.pushConstants(
                m_pipeline.getPipelineLayout(),
                vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
                0,
                sizeof(Craig::PushConstantData),
                &pushData);

            // texture set goes into set 1
            commandBuffer.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                m_pipeline.getPipelineLayout(),
                1, // set 1
                material.m_VK_descriptorSet,
                nullptr);

            commandBuffer.drawIndexed(
                submesh->indexCount,
                1,
                submesh->indexOffset,
                submesh->vertexOffset,
                0);
        }
    }

    for (const Craig::Node* child : node->children)
    {
        drawNode(commandBuffer, model, child, objectIndex);
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
        const Craig::Components::Model* pModelComponent = gameObject->getComponent<Craig::Components::Model>();
        if (pModelComponent == nullptr || !pModelComponent->hasModel()) continue;

        Craig::Model& model = resources.getModel(pModelComponent->getModelPath());
        for (size_t i = 0; i < model.subMeshesCount; i++)
        {
            Craig::SubMesh* submesh = model.subMeshes[i];
            submesh->vertexOffset = totalVertexCount;
            totalVertexCount += static_cast<uint32_t>(submesh->m_vertices.size());
        }
    }

    if (totalVertexCount == 0) {
        Craig::Logger::renderer().debug("No vertices in the scene, skipping the vertex buffer");
        return;
    }

    vk::DeviceSize bufferSize = sizeof(Craig::Vertex) * totalVertexCount;
    Craig::Logger::renderer().debug("Vertex buffer: {} vertices ({:.2f} MB)", totalVertexCount, bufferSize / (1024.0 * 1024.0));

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
        const Craig::Components::Model* pModelComponent = gameObject->getComponent<Craig::Components::Model>();
        if (pModelComponent == nullptr || !pModelComponent->hasModel()) continue;

        const std::string& path = pModelComponent->getModelPath();
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
        const Craig::Components::Model* pModelComponent = gameObject->getComponent<Craig::Components::Model>();
        if (pModelComponent == nullptr || !pModelComponent->hasModel()) continue;

        Craig::Model& model = resources.getModel(pModelComponent->getModelPath());
        for (size_t i = 0; i < model.subMeshesCount; ++i) {
            Craig::SubMesh* submesh = model.subMeshes[i];
            submesh->indexOffset = totalIndexCount;
            totalIndexCount += static_cast<uint32_t>(submesh->m_indices.size());
        }
    }

    if (totalIndexCount == 0) {
        Craig::Logger::renderer().debug("No indices in the scene, skipping the index buffer");
        return;
    }

    vk::DeviceSize bufferSize = sizeof(uint32_t) * totalIndexCount;
    Craig::Logger::renderer().debug("Index buffer: {} indices ({:.2f} MB)", totalIndexCount, bufferSize / (1024.0 * 1024.0));

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
        const Craig::Components::Model* pModelComponent = gameObject->getComponent<Craig::Components::Model>();
        if (pModelComponent == nullptr || !pModelComponent->hasModel()) continue;

        const std::string& path = pModelComponent->getModelPath();
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

// TODO: sub-allocate instead of full rebuild
void Craig::Renderer::rebuildGeometryBuffers() {

    Craig::Logger::renderer().debug("Scene geometry changed, rebuilding the vertex and index buffers");

    // GPU might still be drawing with the old buffers
    m_Devices.getLogicalDevice().waitIdle();

    vmaDestroyBuffer(m_Devices.getVmaAllocator(), m_VK_vertexBuffer, m_VMA_vertexAllocation);
    vmaDestroyBuffer(m_Devices.getVmaAllocator(), m_VK_indexBuffer, m_VMA_indexAllocation);

    // Null them in case the create functions bail early with nothing to upload
    m_VK_vertexBuffer = nullptr;
    m_VMA_vertexAllocation = nullptr;
    m_VK_indexBuffer = nullptr;
    m_VMA_indexAllocation = nullptr;

    createVertexBuffer();
    createIndexBuffer();
}

void Craig::Renderer::createDescriptorPool() {

    std::array<vk::DescriptorPoolSize, 4> poolSizes;
    poolSizes[0]
        .setType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(kMaxFramesInFlight);
    poolSizes[1]
        .setType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(kMaxFramesInFlight);
    poolSizes[2]
        .setType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(kMaxFramesInFlight);
    poolSizes[3]
        .setType(vk::DescriptorType::eCombinedImageSampler)
        .setDescriptorCount(kMaxNumObjects * 2); // 2 per material set, base colour + metallic/roughness


    vk::DescriptorPoolCreateInfo poolInfo{};
    poolInfo
        .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .setPoolSizes(poolSizes)
        .setMaxSets(kMaxFramesInFlight + kMaxNumObjects);

    m_VK_descriptorPool = m_Devices.getLogicalDevice().createDescriptorPool(poolInfo);

}

//for my own sanity
//descriptor sets are basically just the way we pass stuff to the shaders/GPU, so in my case i have 2 descriptor sets, one with the UBO and one with the image sampler
void Craig::Renderer::createDescriptorSets() {

    std::vector<vk::DescriptorSetLayout> perFramelayouts(kMaxFramesInFlight, m_pipeline.getPerFrameDescriptorSetLayout());

    vk::DescriptorSetAllocateInfo perFrameAllocInfo{};
    perFrameAllocInfo.setDescriptorPool(m_VK_descriptorPool)
        .setDescriptorSetCount(kMaxFramesInFlight)
        .setSetLayouts(perFramelayouts);

    mv_VK_perFrameDescriptorSet = m_Devices.getLogicalDevice().allocateDescriptorSets(perFrameAllocInfo);
    std::array<vk::WriteDescriptorSet, 3> perFrameWrites{};

    for (size_t frame = 0; frame < kMaxFramesInFlight; frame++)
    {
        vk::DescriptorBufferInfo cameraBufferInfo{};
        cameraBufferInfo.setBuffer(mv_viewProjUboBuffer[frame])
            .setOffset(0)
            .setRange(sizeof(CameraData));

        vk::DescriptorBufferInfo modelUboBufferInfo{};
        modelUboBufferInfo.setBuffer(mv_VK_storageBuffers[frame])
            .setOffset(0)
            .setRange(sizeof(PerObjectData) * kMaxNumObjects);

        vk::DescriptorBufferInfo lightBufferInfo{};
        lightBufferInfo.setBuffer(mv_lightUboBuffer[frame])
            .setOffset(0)
            .setRange(sizeof(LightData));

        perFrameWrites[0]
            .setDstSet(mv_VK_perFrameDescriptorSet[frame])
            .setDstBinding(0)
            .setDstArrayElement(0)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1)
            .setBufferInfo(cameraBufferInfo);
        perFrameWrites[1]
            .setDstSet(mv_VK_perFrameDescriptorSet[frame])
            .setDstBinding(1)
            .setDstArrayElement(0)
            .setDescriptorType(vk::DescriptorType::eStorageBuffer)
            .setDescriptorCount(1)
            .setBufferInfo(modelUboBufferInfo);
        perFrameWrites[2]
            .setDstSet(mv_VK_perFrameDescriptorSet[frame])
            .setDstBinding(2)
            .setDstArrayElement(0)
            .setDescriptorType(vk::DescriptorType::eUniformBuffer)
            .setDescriptorCount(1)
            .setBufferInfo(lightBufferInfo);

        m_Devices.getLogicalDevice().updateDescriptorSets(perFrameWrites, nullptr);
    }

    createModelDescriptorSets();
}

// One texture set per material (plus each model's default one), only makes sets for materials that don't have one yet
void Craig::Renderer::createModelDescriptorSets() {

    std::vector<Craig::Material*> newMaterials;
    for (auto& [modelPath, model] : Craig::ResourceManager::getInstance().getLoadedModels())
    {
        for (Craig::Material& material : model.materials)
        {
            if (!material.m_VK_descriptorSet)
            {
                newMaterials.push_back(&material);
            }
        }
        if (!model.defaultMaterial.m_VK_descriptorSet)
        {
            newMaterials.push_back(&model.defaultMaterial);
        }
    }

    // allocating 0 sets is invalid in Vulkan
    if (newMaterials.empty()) {
        return;
    }

    std::vector<vk::DescriptorSetLayout> materialLayouts(newMaterials.size(), m_pipeline.getPerObjectDescriptorSetLayout());

    vk::DescriptorSetAllocateInfo materialAllocInfo{};
    materialAllocInfo.setDescriptorPool(m_VK_descriptorPool)
        .setSetLayouts(materialLayouts);

    std::vector<vk::DescriptorSet> materialSets = m_Devices.getLogicalDevice().allocateDescriptorSets(materialAllocInfo);
    for (size_t i = 0; i < newMaterials.size(); i++)
    {
        newMaterials[i]->m_VK_descriptorSet = materialSets[i];
    }

    updateDescriptorSets();
}

void Craig::Renderer::updateDescriptorSets() {

    // called when sets are made or the sampler is recreated (e.g. LOD change)
    // Sets aren't duplicated per frame, so one pass rewriting every material's set is enough
    for (auto& [modelPath, model] : Craig::ResourceManager::getInstance().getLoadedModels())
    {
        auto writeMaterial = [&](const Craig::Material& material)
        {
            std::array<vk::DescriptorImageInfo, 2> imageInfos{};
            imageInfos[0]
                .setImageView(model.getMaterialImage(material).m_VK_textureImageView)
                .setSampler(m_VK_textureSampler)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
            imageInfos[1]
                .setImageView(model.getMetallicRoughnessImage(material).m_VK_textureImageView)
                .setSampler(m_VK_textureSampler)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            // bindings 0 and 1 are next to each other so one write with a count of 2 fills both
            vk::WriteDescriptorSet descriptorWrite{};
            descriptorWrite
                .setDstSet(material.m_VK_descriptorSet)
                .setDstBinding(0)
                .setDstArrayElement(0)
                .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                .setImageInfo(imageInfos);

            m_Devices.getLogicalDevice().updateDescriptorSets(descriptorWrite, nullptr);
        };

        for (const Craig::Material& material : model.materials)
        {
            writeMaterial(material);
        }
        writeMaterial(model.defaultMaterial);
    }

}

// Sets up our two GPU buffers: the big SSBO holding every object's model matrix, and a tiny UBO for the camera's view/proj.
// We make kMaxFramesInFlight copies of each so the CPU and GPU aren't fighting over the same memory.
void Craig::Renderer::createUniformBuffers() {

    // Host visible + mapped so we can just write into it from the CPU every frame, no staging buffer needed.
    VmaAllocationCreateInfo stagingAci{};
    stagingAci.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAci.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    stagingAci.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // The SSBO: one big array holding per-object data (just model matrix for now). One buffer per frame-in-flight,
    // oversized for kMaxNumObjects so adding/removing gameobjects doesn't need a reallocation.
    mv_VK_storageBuffers.resize(kMaxFramesInFlight);
    mv_VK_storageBuffersAllocations.resize(kMaxFramesInFlight);
    mv_VK_storageBuffersMapped.resize(kMaxFramesInFlight);

    vk::DeviceSize storageBufferSize = kMaxNumObjects * sizeof(PerObjectData);
    for (size_t i = 0; i < kMaxFramesInFlight; i++)
    {
        VmaAllocationInfo info{};
        m_Devices.createBufferVMA(storageBufferSize, vk::BufferUsageFlagBits::eStorageBuffer, stagingAci, mv_VK_storageBuffers[i], mv_VK_storageBuffersAllocations[i], &info);

        mv_VK_storageBuffersMapped[i] = info.pMappedData;
    }


    // The camera UBO: tiny, just view + proj. Still one per frame-in-flight though, the camera moves every frame so
    // the GPU might still be reading last frame's copy while we write the new one.
    mv_viewProjUboBuffer.resize(kMaxFramesInFlight);
    mv_viewProjUboAllocation.resize(kMaxFramesInFlight);
    mv_viewProjUboMap.resize(kMaxFramesInFlight);

    vk::DeviceSize viewProjBufferSize = sizeof(CameraData);
    for (size_t i = 0; i < kMaxFramesInFlight; i++)
    {
        VmaAllocationInfo info2{};
        m_Devices.createBufferVMA(viewProjBufferSize, vk::BufferUsageFlagBits::eUniformBuffer, stagingAci, mv_viewProjUboBuffer[i], mv_viewProjUboAllocation[i], &info2);
        mv_viewProjUboMap[i] = info2.pMappedData;
    }

    //Light UBO, practically the same as the camera UBO
    mv_lightUboBuffer.resize(kMaxFramesInFlight);
    mv_lightUboAllocation.resize(kMaxFramesInFlight);
    mv_lightUboMap.resize(kMaxFramesInFlight);

    vk::DeviceSize lightBufferSize = sizeof(LightData);
    for (size_t i = 0; i < kMaxFramesInFlight; i++)
    {
        VmaAllocationInfo info3{};
        m_Devices.createBufferVMA(lightBufferSize, vk::BufferUsageFlagBits::eUniformBuffer, stagingAci, mv_lightUboBuffer[i], mv_lightUboAllocation[i], &info3);
        mv_lightUboMap[i] = info3.pMappedData;
    }


}

// UBO deals with where a thing is and how to project it, but the thing itself is held within the vertex buffer.
// Only writes to currentImage's buffers cos the other frame-in-flight copies might still be in use by the GPU.
void Craig::Renderer::updateUniformBuffer(uint32_t currentImage, const float& deltaTime) {

    Craig::Camera& camera = mp_SceneManager->getCurrentScene()->getCamera();

    camera.m_aspect = m_swapChain.getExtent().width / (float)m_swapChain.getExtent().height;

    std::vector<Craig::GameObject*>& currentSceneObjects = mp_SceneManager->getCurrentScene()->getGameObjects();
    Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();

    camera.update(deltaTime);

    // Write each gameobject's current model matrix into its slot in this frame's SSBO.
    // The shader will index into this array to grab the right transform for the object it's drawing.
    auto* dst = static_cast<PerObjectData*>(mv_VK_storageBuffersMapped[currentImage]);
    for (size_t gObj = 0; gObj < currentSceneObjects.size(); gObj++)
    {
        dst[gObj].model = currentSceneObjects[gObj]->GetModelMatrix();
    }


    // View and proj are the same for every object this frame, so we write them once into the camera UBO rather than
    // stuffing a copy into every object's slot.
    CameraData viewProjUbo;
    viewProjUbo.view = camera.getView();
    viewProjUbo.proj = camera.getProj();
    viewProjUbo.camPos = glm::vec4(camera.getPosition(), 1.0f);
    memcpy(mv_viewProjUboMap[currentImage], &viewProjUbo, sizeof(viewProjUbo));

    LightData lightData;
    const Craig::Components::Sun* pSun = mp_SceneManager->getCurrentScene()->getSun();
    if (pSun != nullptr)
    {
        lightData.lightDir = glm::vec4(pSun->getLightDir(), 0.0f);
        lightData.lightColour = glm::vec4(pSun->getLightColour(), pSun->getIntensity());
        lightData.skyColour = glm::vec4(pSun->getSkyColour(), 0.0f);
        lightData.groundColour = glm::vec4(pSun->getGroundColour(), 0.0f);
    }
    else
    {
        // No sun means no directional light, keep a bit of ambient so the scene isn't pitch black
        lightData.lightDir = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        lightData.lightColour = glm::vec4(0.0f);
        lightData.skyColour = glm::vec4(0.05f, 0.05f, 0.08f, 0.0f);
        lightData.groundColour = lightData.skyColour;
    }
    memcpy(mv_lightUboMap[currentImage], &lightData, sizeof(lightData));


}

void Craig::Renderer::createTextureImage2(const uint8_t* pixels, int texWidth, int texHeight, int texChannels, Craig::Texture* outTexture, bool srgb) { // VmaAllocation* textureMemoryAlloc, vk::Image* outTextureImage, vk::ImageView* outTextureImageView) {
    vk::DeviceSize imageSize = texWidth * texHeight * 4;
    const vk::Format format = srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm;

    if (!pixels) {
        Craig::Logger::renderer().critical("Tried to make a texture with no pixels");
        throw std::runtime_error("failed to load texture image!");
    }

    outTexture->m_VK_mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
    Craig::Logger::renderer().debug("Uploading a {} x {} texture ({} mips)", texWidth, texHeight, outTexture->m_VK_mipLevels);

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

    outTexture->m_VK_textureImage = ImageHelpers::createImage(m_Devices.getPhysicalDevice(), m_instance.getVkSurface(), texWidth, texHeight, outTexture->m_VK_mipLevels, vk::SampleCountFlagBits::e1, format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, m_Devices.getVmaAllocator(),outTexture->m_VMA_textureImageAllocation);

    Craig::ImageHelpers::transitionImageLayout(m_commandManager ,outTexture->m_VK_textureImage, format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, true, outTexture->m_VK_mipLevels);
    Craig::ImageHelpers::copyBufferToImage(m_commandManager, stagingBuffer, outTexture->m_VK_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    //transitionImageLayout(m_VK_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal, false, m_VK_mipLevels); <- now done when generating mipMaps

    vmaDestroyBuffer(m_Devices.getVmaAllocator(), stagingBuffer, stagingAlloc);

    Craig::ImageHelpers::generateMipMaps(m_commandManager, m_Devices.getPhysicalDevice().getFormatProperties(format), outTexture->m_VK_textureImage, texWidth, texHeight, outTexture->m_VK_mipLevels, false);

    outTexture->m_VK_textureImageView = Craig::ImageHelpers::createImageView(m_Devices.getLogicalDevice(), outTexture->m_VK_textureImage, format, vk::ImageAspectFlagBits::eColor, outTexture->m_VK_mipLevels);

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
    Craig::Logger::renderer().debug("Texture sampler made ({}x anisotropy, min LOD {})", physicalDeviceProperties.limits.maxSamplerAnisotropy, m_minLODLevel);



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
        Craig::Logger::renderer().warn("{}x isn't a valid MSAA level, keeping {}", levelToSet, vk::to_string(m_renderingAttachments.m_VK_msaaSamples));
        break;
    }
    Craig::Logger::renderer().info("MSAA set to {}", vk::to_string(m_renderingAttachments.m_VK_msaaSamples));
    recreateSwapChainFull();

}

void Craig::Renderer::deleteGameObject(Craig::GameObject* gameObject)
{
    //gotta wait for the object to leave the command buffer or vulkan cries with validation error
    m_Devices.getLogicalDevice().waitIdle();
    // texture sets belong to the model now, so nothing to free here
    // Remove from the scene and delete the object itself.
    mp_SceneManager->getCurrentScene()->deleteGameObject(gameObject);

}

CraigError Craig::Renderer::newGameObject(std::string objectName, std::string modelPath, glm::vec3 position)
{
    CraigError ret = CRAIG_SUCCESS;

    // The model component marks the scene's geometry dirty, update() rebuilds the buffers before the next draw
    ret = mp_SceneManager->getCurrentScene()->newGameObject(objectName, modelPath, position);

    return ret;
}

CraigError Craig::Renderer::loadScene(const std::string& scenePath)
{
    CraigError ret = CRAIG_SUCCESS;

    // GPU might still be using the old scene's buffers + sets
    m_Devices.getLogicalDevice().waitIdle();

    ret = mp_SceneManager->loadScene(scenePath);
    if (ret != CRAIG_SUCCESS)
    {
        Craig::Logger::renderer().error("Couldn't switch to {}, keeping the current scene", scenePath);
        return ret;
    }

    // different scene, so nothing to reselect
    finishSceneSwap("");

    return ret;
}

CraigError Craig::Renderer::restoreScene(const nlohmann::json& sceneJson, const std::string& scenePath)
{
    CraigError ret = CRAIG_SUCCESS;

    // grab the name first, the pointer's dead after the swap
    std::string selectedName;
#if defined(IMGUI_ENABLED)
    selectedName = Craig::ImguiEditor::getInstance().getSelectedGameObjectName();
#endif

    // GPU might still be using the old scene's buffers + sets
    m_Devices.getLogicalDevice().waitIdle();

    ret = mp_SceneManager->loadSceneFromJson(sceneJson, scenePath);
    if (ret != CRAIG_SUCCESS)
    {
        Craig::Logger::renderer().error("Couldn't rebuild {} from memory, keeping what's there", scenePath);
        return ret;
    }

    finishSceneSwap(selectedName);

    return ret;
}

void Craig::Renderer::finishSceneSwap(const std::string& reselectObjectName)
{
    // Buffers are built from the scene's objects, so remake them + sets for any new models
    rebuildGeometryBuffers();
    createModelDescriptorSets();

    // camera lives in the scene, so point everyone at the new one
    mp_CurrentWindow->setCameraRef(&mp_SceneManager->getCurrentScene()->getCamera());
#if defined(IMGUI_ENABLED)
    Craig::ImguiEditor::getInstance().setCamera(&mp_SceneManager->getCurrentScene()->getCamera());
    // the editor's still pointing at stuff from the old scene
    Craig::ImguiEditor::getInstance().onSceneSwapped(reselectObjectName);
#else
    (void)reselectObjectName;
#endif
}

void Craig::Renderer::updateMinLOD(int minLOD) {
    m_minLODLevel = minLOD;

    terminateSampler();
    createTextureSampler();
    updateDescriptorSets();

    Craig::Logger::renderer().info("Min LOD set to {}, recreated the sampler and updated the descriptor sets", minLOD);
}

void Craig::Renderer::drawFrame(const float& deltaTime) {

    {
        CRAIG_PROFILE_SCOPE("  Wait for GPU");
        m_syncManager.waitForGpu();
    }
    const uint32_t& currentFrame = m_syncManager.getCurrentFrame();

    if (m_swapChain.getExtent().width <= 0 || m_swapChain.getExtent().height <= 0) {
        return; // Skip this frame
    }

    uint32_t imageIndex = 0;
    VkResult nextImageResult;
    {
        CRAIG_PROFILE_SCOPE("  Acquire image");
        nextImageResult = vkAcquireNextImageKHR(m_Devices.getLogicalDevice(), m_swapChain.getSwapChain(), UINT64_MAX, m_syncManager.getVK_imageAvailableSemaphores()[currentFrame], VK_NULL_HANDLE, &imageIndex);
    }

    if (nextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    }
    else if (nextImageResult != VK_SUCCESS && nextImageResult != VK_SUBOPTIMAL_KHR) {
        Craig::Logger::renderer().critical("Failed to acquire a swapchain image: {}", vk::to_string(vk::Result(nextImageResult)));
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // Record drawing commands into the command buffer
    {
        CRAIG_PROFILE_SCOPE("  Record commands");
        m_commandManager.getCommandBuffers()[currentFrame].reset();
        recordCommandBuffer(m_commandManager.getCommandBuffers()[currentFrame], imageIndex);
    }

    {
        CRAIG_PROFILE_SCOPE("  Update UBOs");
        updateUniformBuffer(currentFrame, deltaTime);
    }

    //Creates the submit info and submits the command buffer to the gfx queue
    {
        CRAIG_PROFILE_SCOPE("  Submit");
        m_syncManager.submitFrame(m_commandManager.getCommandBuffers(), imageIndex, m_Devices.getGraphicsQueue());
    }

    // Present the rendered image to the screen
    vk::PresentInfoKHR presentInfo;
    presentInfo
        .setWaitSemaphoreCount(1)
        .setPWaitSemaphores(&m_syncManager.getVK_renderFinishedSemaphores()[imageIndex])
        .setSwapchainCount(1)
        .setPSwapchains(&m_swapChain.getSwapChain())
        .setPImageIndices(&imageIndex);


    //We have to revert back to the original C code otherwise if it returns ERROR_OUT_OF_DATE, it throws an exception and messes up the code.
    VkResult presentResult;
    {
        CRAIG_PROFILE_SCOPE("  Present");
        presentResult = vkQueuePresentKHR(m_Devices.getPresentationQueue(), presentInfo);
    }

    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || mp_CurrentWindow->isResizeNeeded()) {
        recreateSwapChain();
        mp_CurrentWindow->finishedResize();
    }
    else if (presentResult != VK_SUCCESS) {
        Craig::Logger::renderer().critical("Failed to present: {}", vk::to_string(vk::Result(presentResult)));
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_syncManager.nextFrame();

}

CraigError Craig::Renderer::terminate() {

    CraigError ret = CRAIG_SUCCESS;

    m_Devices.getLogicalDevice().waitIdle();

#if defined(IMGUI_ENABLED)
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
#endif
    vmaDestroyBuffer(m_Devices.getVmaAllocator(), m_VK_indexBuffer, m_VMA_indexAllocation);

    vmaDestroyBuffer(m_Devices.getVmaAllocator(), m_VK_vertexBuffer, m_VMA_vertexAllocation);

    m_syncManager.terminate();

    m_commandManager.terminate();

    m_renderingAttachments.terminate();

    m_Devices.getLogicalDevice().destroySampler(m_VK_textureSampler);

    Craig::ResourceManager::getInstance().terminateModels(m_Devices.getLogicalDevice(), m_Devices.getVmaAllocator());



    m_swapChain.terminate();

    for (size_t i = 0; i < mv_VK_storageBuffers.size(); i++) {
        vmaDestroyBuffer(m_Devices.getVmaAllocator(), mv_VK_storageBuffers[i], mv_VK_storageBuffersAllocations[i]);
    }

    for (size_t i = 0; i < mv_viewProjUboBuffer.size(); i++) {
        vmaDestroyBuffer(m_Devices.getVmaAllocator(), mv_viewProjUboBuffer[i], mv_viewProjUboAllocation[i]);
    }

    for (size_t i = 0; i < mv_lightUboBuffer.size(); i++) {
        vmaDestroyBuffer(m_Devices.getVmaAllocator(), mv_lightUboBuffer[i], mv_lightUboAllocation[i]);
    }

    m_Devices.getLogicalDevice().destroyDescriptorPool(m_VK_descriptorPool);

    m_pipeline.terminate();


    m_Devices.terminate();

    m_instance.terminate();

    Craig::Logger::renderer().info("Renderer shut down");

    return ret;
}

void Craig::Renderer::terminateSampler() {
    m_Devices.getLogicalDevice().waitIdle();
    m_Devices.getLogicalDevice().destroySampler(m_VK_textureSampler);
}
