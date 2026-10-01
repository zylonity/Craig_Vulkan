#include "Craig_Pipeline.hpp"
#include "Craig/Craig_Logger.hpp"

CraigError Craig::Pipeline::init(const PipelineInitInfo& info) {

	CraigError ret = CRAIG_SUCCESS;

    mPipe_device = info.device;
    mPipe_colorFormat = info.colorFormat;
    mPipe_depthFormat = info.depthFormat;
    mPipe_msaaSamples = info.msaaSamples;

    createDescriptorSetLayout();
    createGraphicsPipeline();
    createSkyPipeline();

	return ret;
}

void Craig::Pipeline::createGraphicsPipeline() {

    // Load the SPIR-V that glslc compiled from the GLSL at build time
    m_VK_vertShaderModule = Craig::ShaderCompilation::LoadShaderModule(mPipe_device, "data/shaders/vert.spv");
    m_VK_fragShaderModule = Craig::ShaderCompilation::LoadShaderModule(mPipe_device, "data/shaders/frag.spv");


    // Set up shader stages for the pipeline
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo
        .setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(m_VK_vertShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo
        .setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(m_VK_fragShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    vk::VertexInputBindingDescription                   bindingDescription = Vertex::getBindingDescription();
    std::array<vk::VertexInputAttributeDescription, kVertexAttributeDescriptors>  attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo
        .setVertexBindingDescriptionCount(1)
        .setPVertexBindingDescriptions(&bindingDescription) //These should point to an array of structs w vertex descriptions
        .setVertexAttributeDescriptionCount(static_cast<uint32_t>(attributeDescriptions.size()))
        .setPVertexAttributeDescriptions(attributeDescriptions.data());

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly
        .setTopology(vk::PrimitiveTopology::eTriangleList)
        .setPrimitiveRestartEnable(vk::False);

    // Viewport/scissor are dynamic (set later in the command buffer)
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState
        .setViewportCount(1)
        .setScissorCount(1);

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer
        .setDepthClampEnable(vk::False)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setLineWidth(1.0f)
        .setCullMode(vk::CullModeFlagBits::eBack)
        .setFrontFace(vk::FrontFace::eCounterClockwise)
        .setDepthBiasEnable(false);

    //Multisampling/Anti-Aliasing
    //Keeping it disabled for now but will follow up later in the tutorial
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling
        .setSampleShadingEnable(vk::False)
        .setRasterizationSamples(*mPipe_msaaSamples);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil
        .setDepthTestEnable(true)
        .setDepthWriteEnable(true)
        .setDepthCompareOp(vk::CompareOp::eLess)
        .setDepthBoundsTestEnable(false)
        .setStencilTestEnable(false);

    vk::PipelineColorBlendAttachmentState colourBlendAttachment{};
    colourBlendAttachment
        .setColorWriteMask(
        vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA)
        .setBlendEnable(vk::False);

    vk::PipelineColorBlendStateCreateInfo colourBlending{};
    colourBlending
        .setLogicOpEnable(vk::False)
        .setLogicOp(vk::LogicOp::eCopy)
        .setAttachmentCount(1)
        .setPAttachments(&colourBlendAttachment);

    //Some bits of the pipeline can be changed, like the viewport, without having to recreate the pipeline/bake them again
    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState
        .setDynamicStateCount(static_cast<uint32_t>(dynamicStates.size()))
        .setPDynamicStates(dynamicStates.data());


    vk::PushConstantRange pushRange{};
    pushRange
        .setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment) // fragment reads the material colour
        .setOffset(0)
        .setSize(sizeof(Craig::PushConstantData));

    std::array setLayouts = { m_VK_perFrameSetLayout, m_VK_perObjectSetLayout };
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo
        .setSetLayouts(setLayouts)
        .setPushConstantRanges(pushRange);

    



    try {
        m_VK_pipelineLayout = mPipe_device.createPipelineLayout(pipelineLayoutInfo);
    }
    catch (const vk::SystemError& err) {
        Craig::Logger::renderer().critical("Failed to create the pipeline layout: {}", err.what());
        throw std::runtime_error("failed to createPipelineLayout!");
    }

    vk::Format colorFormat = mPipe_colorFormat;
    vk::Format depthFormat = mPipe_depthFormat;

    vk::PipelineRenderingCreateInfo renderingInfo{};
    renderingInfo
        .setColorAttachmentCount(1)
        .setPColorAttachmentFormats(&colorFormat)
        .setDepthAttachmentFormat(depthFormat);


    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo
        .setPNext(&renderingInfo)
        .setStageCount(2)
        .setPStages(shaderStages)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPDepthStencilState(&depthStencil)
        .setPColorBlendState(&colourBlending)
        .setPDynamicState(&dynamicState)
        .setLayout(m_VK_pipelineLayout)
        .setRenderPass(VK_NULL_HANDLE); //Needs to be null as we're using a dynamic renderer


    auto result = mPipe_device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo);

    if (result.result != vk::Result::eSuccess) {
        Craig::Logger::renderer().critical("Failed to create the graphics pipeline: {}", vk::to_string(result.result));
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
    m_VK_graphicsPipeline = result.value;

    Craig::Logger::renderer().info("Graphics pipeline made ({} colour, {} depth, {} MSAA)", vk::to_string(mPipe_colorFormat), vk::to_string(mPipe_depthFormat), vk::to_string(*mPipe_msaaSamples));


}

// Mostly the same as the main pipeline, the differences are:
// no vertex buffer (the triangle comes from gl_VertexIndex), no culling, and depth is test only at LessOrEqual
void Craig::Pipeline::createSkyPipeline() {

    m_VK_skyVertShaderModule = Craig::ShaderCompilation::LoadShaderModule(mPipe_device, "data/shaders/skyVert.spv");
    m_VK_skyFragShaderModule = Craig::ShaderCompilation::LoadShaderModule(mPipe_device, "data/shaders/skyFrag.spv");

    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo
        .setStage(vk::ShaderStageFlagBits::eVertex)
        .setModule(m_VK_skyVertShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo
        .setStage(vk::ShaderStageFlagBits::eFragment)
        .setModule(m_VK_skyFragShaderModule)
        .setPName("main");

    vk::PipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    // nothing to feed in, the vert shader makes its own 3 corners
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly
        .setTopology(vk::PrimitiveTopology::eTriangleList)
        .setPrimitiveRestartEnable(vk::False);

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState
        .setViewportCount(1)
        .setScissorCount(1);

    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer
        .setDepthClampEnable(vk::False)
        .setPolygonMode(vk::PolygonMode::eFill)
        .setLineWidth(1.0f)
        .setCullMode(vk::CullModeFlagBits::eNone)
        .setFrontFace(vk::FrontFace::eCounterClockwise)
        .setDepthBiasEnable(false);

    // has to match the main pipeline, they draw into the same attachments
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling
        .setSampleShadingEnable(vk::False)
        .setRasterizationSamples(*mPipe_msaaSamples);

    // The sky sits at depth 1 and depth gets cleared to 1, so with plain Less it'd never pass
    // No writing either, it's the background
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil
        .setDepthTestEnable(true)
        .setDepthWriteEnable(false)
        .setDepthCompareOp(vk::CompareOp::eLessOrEqual)
        .setDepthBoundsTestEnable(false)
        .setStencilTestEnable(false);

    vk::PipelineColorBlendAttachmentState colourBlendAttachment{};
    colourBlendAttachment
        .setColorWriteMask(
        vk::ColorComponentFlagBits::eR |
        vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB |
        vk::ColorComponentFlagBits::eA)
        .setBlendEnable(vk::False);

    vk::PipelineColorBlendStateCreateInfo colourBlending{};
    colourBlending
        .setLogicOpEnable(vk::False)
        .setLogicOp(vk::LogicOp::eCopy)
        .setAttachmentCount(1)
        .setPAttachments(&colourBlendAttachment);

    std::vector<vk::DynamicState> dynamicStates = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor
    };

    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState
        .setDynamicStateCount(static_cast<uint32_t>(dynamicStates.size()))
        .setPDynamicStates(dynamicStates.data());

    vk::Format colorFormat = mPipe_colorFormat;
    vk::Format depthFormat = mPipe_depthFormat;

    vk::PipelineRenderingCreateInfo renderingInfo{};
    renderingInfo
        .setColorAttachmentCount(1)
        .setPColorAttachmentFormats(&colorFormat)
        .setDepthAttachmentFormat(depthFormat);

    // same layout as the main pipeline, so set 0 (camera + sun) stays bound between them
    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo
        .setPNext(&renderingInfo)
        .setStageCount(2)
        .setPStages(shaderStages)
        .setPVertexInputState(&vertexInputInfo)
        .setPInputAssemblyState(&inputAssembly)
        .setPViewportState(&viewportState)
        .setPRasterizationState(&rasterizer)
        .setPMultisampleState(&multisampling)
        .setPDepthStencilState(&depthStencil)
        .setPColorBlendState(&colourBlending)
        .setPDynamicState(&dynamicState)
        .setLayout(m_VK_pipelineLayout)
        .setRenderPass(VK_NULL_HANDLE);

    auto result = mPipe_device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo);

    if (result.result != vk::Result::eSuccess) {
        Craig::Logger::renderer().critical("Failed to create the sky pipeline: {}", vk::to_string(result.result));
        throw std::runtime_error("Failed to create sky pipeline!");
    }
    m_VK_skyPipeline = result.value;

    Craig::Logger::renderer().info("Sky pipeline made");
}

void Craig::Pipeline::cleanupGraphicsPipeline() {

    if (m_VK_graphicsPipeline) {
        mPipe_device.destroyPipeline(m_VK_graphicsPipeline);
        m_VK_graphicsPipeline = nullptr;
    }

    if (m_VK_skyPipeline) {
        mPipe_device.destroyPipeline(m_VK_skyPipeline);
        m_VK_skyPipeline = nullptr;
    }

    if (m_VK_skyVertShaderModule) {
        mPipe_device.destroyShaderModule(m_VK_skyVertShaderModule);
        m_VK_skyVertShaderModule = nullptr;
    }

    if (m_VK_skyFragShaderModule) {
        mPipe_device.destroyShaderModule(m_VK_skyFragShaderModule);
        m_VK_skyFragShaderModule = nullptr;
    }

    if (m_VK_pipelineLayout) {
        mPipe_device.destroyPipelineLayout(m_VK_pipelineLayout);
        m_VK_pipelineLayout = nullptr;
    }

    if (m_VK_vertShaderModule) {
        mPipe_device.destroyShaderModule(m_VK_vertShaderModule);
        m_VK_vertShaderModule = nullptr;
    }

    if (m_VK_fragShaderModule) {
        mPipe_device.destroyShaderModule(m_VK_fragShaderModule);
        m_VK_fragShaderModule = nullptr;
    }

}


//From vulkan-tutorial.com
//The descriptor set layout specifies the types of resources that are going to be accessed by the pipeline, just like a render pass specifies the types of attachments that will be accessed.
//
//A descriptor set specifies the actual buffer or image resources that will be bound to the descriptors, just like a framebuffer specifies the actual image views to bind to render pass attachments.
void Craig::Pipeline::createDescriptorSetLayout() {


    vk::DescriptorSetLayoutBinding cameraLayoutBinding{};
    cameraLayoutBinding
        .setBinding(0)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment); // frag needs camPos

    vk::DescriptorSetLayoutBinding storageBufferLayoutBinding{};
    storageBufferLayoutBinding
        .setBinding(1)
        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex);

    vk::DescriptorSetLayoutBinding lightBinding{};
    lightBinding
        .setBinding(2)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment);

    std::array<vk::DescriptorSetLayoutBinding, 3> perFrameBindings = { cameraLayoutBinding, storageBufferLayoutBinding, lightBinding };

    vk::DescriptorSetLayoutCreateInfo perFrameLayoutInfo{};
    perFrameLayoutInfo
        .setBindingCount(static_cast<uint32_t>(perFrameBindings.size()))
        .setBindings(perFrameBindings);

    m_VK_perFrameSetLayout = mPipe_device.createDescriptorSetLayout(perFrameLayoutInfo);

    vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding
        .setBinding(0)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setPImmutableSamplers(nullptr)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment);

    // metallic (B) + roughness (G), glTF packs them into one texture
    vk::DescriptorSetLayoutBinding metallicRoughnessLayoutBinding{};
    metallicRoughnessLayoutBinding
        .setBinding(1)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setPImmutableSamplers(nullptr)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment);

    // one of these per material really, not per object
    std::array<vk::DescriptorSetLayoutBinding, 2> perObjectBindings = { samplerLayoutBinding, metallicRoughnessLayoutBinding };

    vk::DescriptorSetLayoutCreateInfo perObjectLayoutInfo{};
    perObjectLayoutInfo
        .setBindingCount(static_cast<uint32_t>(perObjectBindings.size()))
        .setBindings(perObjectBindings);

    m_VK_perObjectSetLayout = mPipe_device.createDescriptorSetLayout(perObjectLayoutInfo);


}

CraigError Craig::Pipeline::terminate() {

	CraigError ret = CRAIG_SUCCESS;

    cleanupGraphicsPipeline();
    mPipe_device.destroyDescriptorSetLayout(m_VK_perFrameSetLayout);
    mPipe_device.destroyDescriptorSetLayout(m_VK_perObjectSetLayout);

	return ret;
}

void Craig::Pipeline::recreate()
{
    Craig::Logger::renderer().debug("Recreating the graphics pipeline");
    cleanupGraphicsPipeline();
    createGraphicsPipeline();
    createSkyPipeline();
}


