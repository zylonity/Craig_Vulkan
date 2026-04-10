#include "Craig_CommandManager.hpp"

#include "Craig_Device.hpp"

CraigError Craig::CommandManager::init(const CommandManagerInitInfo& info) {

	CraigError ret = CRAIG_SUCCESS;

	mp_Device = info.p_Device;
	m_CM_surface = info.surface;


	return ret;
}

void Craig::CommandManager::createCommandPool() {

	Device::QueueFamilyIndices queueFamilyIndices = Device::findQueueFamilies(mp_Device->getPhysicalDevice(), m_CM_surface);

	vk::CommandPoolCreateInfo poolInfo{};
	poolInfo
		.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer)
		.setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value());

	m_VK_commandPool = mp_Device->getLogicalDevice().createCommandPool(poolInfo);

	if (queueFamilyIndices.hasDedicatedTransfer()) {
		vk::CommandPoolCreateInfo info{};
		info
			.setQueueFamilyIndex(queueFamilyIndices.transferFamily.value())
			.setFlags(vk::CommandPoolCreateFlagBits::eTransient); // copies are short-lived

		m_VK_transferCommandPool = mp_Device->getLogicalDevice().createCommandPool(info);
	}
	else {
		// No dedicated transfer family � reuse graphics pool
		m_VK_transferCommandPool = m_VK_commandPool;
	}

}

void Craig::CommandManager::createCommandBuffers() {
	mv_VK_commandBuffers.resize(kMaxFramesInFlight);

	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo
		.setCommandPool(m_VK_commandPool)
		.setLevel(vk::CommandBufferLevel::ePrimary)
		.setCommandBufferCount((uint32_t)mv_VK_commandBuffers.size());

	try {
		mv_VK_commandBuffers = mp_Device->getLogicalDevice().allocateCommandBuffers(allocInfo);
	}
	catch (const vk::SystemError& err) {
		throw std::runtime_error("failed to allocate command buffers!");
	}


}

vk::CommandBuffer Craig::CommandManager::buffer_beginSingleTimeCommands() {
    //Allocate a temporary command buffer
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(m_VK_transferCommandPool)
        .setCommandBufferCount(1);


    vk::CommandBuffer commandBuffer = mp_Device->getLogicalDevice().allocateCommandBuffers(allocInfo)[0];

    //Start recording the command buffer
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Craig::CommandManager::buffer_endSingleTimeCommands(vk::CommandBuffer commandBuffer) {

    //Stop recording
    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBufferCount(1)
        .setCommandBuffers(commandBuffer);

    mp_Device->getTransferQueue().submit(submitInfo);
    mp_Device->getTransferQueue().waitIdle();

    mp_Device->getLogicalDevice().freeCommandBuffers(m_VK_transferCommandPool, commandBuffer);
}

vk::CommandBuffer Craig::CommandManager::buffer_beginSingleTimeCommandsGFX() {
    //Allocate a temporary command buffer
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary)
        .setCommandPool(m_VK_commandPool)
        .setCommandBufferCount(1);


    vk::CommandBuffer commandBuffer = mp_Device->getLogicalDevice().allocateCommandBuffers(allocInfo)[0];

    //Start recording the command buffer
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Craig::CommandManager::buffer_endSingleTimeCommandsGFX(vk::CommandBuffer commandBuffer) {

    //Stop recording
    commandBuffer.end();

    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBufferCount(1)
        .setCommandBuffers(commandBuffer);

    mp_Device->getGraphicsQueue().submit(submitInfo);
    mp_Device->getGraphicsQueue().waitIdle();

    mp_Device->getLogicalDevice().freeCommandBuffers(m_VK_commandPool, commandBuffer);
}

void Craig::CommandManager::copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {

	//Begin recording to buffer
	vk::CommandBuffer tempBuffer = buffer_beginSingleTimeCommands();

	//Copy over the data
	vk::BufferCopy copyRegion{};
	copyRegion.setSize(size);
	tempBuffer.copyBuffer(srcBuffer, dstBuffer, copyRegion);

	//End recording and submit buffer
	buffer_endSingleTimeCommands(tempBuffer);


}

CraigError Craig::CommandManager::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::CommandManager::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	if (m_VK_transferCommandPool && m_VK_transferCommandPool != m_VK_commandPool) {
	    mp_Device->getLogicalDevice().destroyCommandPool(m_VK_transferCommandPool);
	}

	mp_Device->getLogicalDevice().destroyCommandPool(m_VK_commandPool);

	return ret;
}


