#include "Craig_Pipeline.hpp"

CraigError Craig::Pipeline::init(const PipelineInitInfo& info) {

	CraigError ret = CRAIG_SUCCESS;

    mPipe_device = info.device;
    mPipe_colorFormat = info.colorFormat;
    mPipe_depthFormat = info.depthFormat;
    mPipe_msaaSamples = info.msaaSamples;

    createDescriptorSetLayout();
    createGraphicsPipeline();

	return ret;
}

void Craig::Pipeline::createGraphicsPipeline() {

    // Compile HLSL shaders to SPIR-V shader modules
#if defined(_WIN32)
    m_VK_vertShaderModule = Craig::ShaderCompilation::CompileHLSLToShaderModule(mPipe_device, L"data/shaders/VertexShader.vert");
    m_VK_fragShaderModule = Craig::ShaderCompilation::CompileHLSLToShaderModule(mPipe_device, L"data/shaders/FragmentShader.frag");
#elif defined(__APPLE__) || defined(__linux__)

    m_VK_vertShaderModule = Craig::ShaderCompilation::CompileHLSLToShaderModule(mPipe_device, L"data/shaders/vert.spv");
    m_VK_fragShaderModule = Craig::ShaderCompilation::CompileHLSLToShaderModule(mPipe_device, L"data/shaders/frag.spv");
#endif



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
    std::array<vk::VertexInputAttributeDescription, 3>  attributeDescriptions = Vertex::getAttributeDescriptions();

    //No vertex data to load for now since its hardcoded into the shader.
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
        .setRasterizationSamples(mPipe_msaaSamples);

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


    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setSetLayoutCount(1)
        .setSetLayouts(m_VK_descriptorSetLayout);

    try {
        m_VK_pipelineLayout = mPipe_device.createPipelineLayout(pipelineLayoutInfo);
    }
    catch (const vk::SystemError& err) {
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
        throw std::runtime_error("Failed to create graphics pipeline!");
    }
    m_VK_graphicsPipeline = result.value;


}

void Craig::Pipeline::cleanupGraphicsPipeline() {

    if (m_VK_graphicsPipeline) {
        mPipe_device.destroyPipeline(m_VK_graphicsPipeline);
        m_VK_graphicsPipeline = nullptr;
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

    vk::DescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding
        .setBinding(0)
        .setDescriptorType(vk::DescriptorType::eUniformBuffer)
        .setDescriptorCount(1)
        .setStageFlags(vk::ShaderStageFlagBits::eVertex);

    vk::DescriptorSetLayoutBinding samplerLayourBinding{};
    samplerLayourBinding
        .setBinding(1)
        .setDescriptorCount(1)
        .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
        .setPImmutableSamplers(nullptr)
        .setStageFlags(vk::ShaderStageFlagBits::eFragment);

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayourBinding };

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo
        .setBindingCount(static_cast<uint32_t>(bindings.size()))
        .setBindings(bindings);

    m_VK_descriptorSetLayout = mPipe_device.createDescriptorSetLayout(layoutInfo);


}

CraigError Craig::Pipeline::terminate() {

	CraigError ret = CRAIG_SUCCESS;

    cleanupGraphicsPipeline();
    mPipe_device.destroyDescriptorSetLayout(m_VK_descriptorSetLayout);

	return ret;
}

void Craig::Pipeline::recreate()
{
    cleanupGraphicsPipeline();
    createGraphicsPipeline();
}


