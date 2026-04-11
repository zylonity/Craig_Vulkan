#include "Craig_SyncManager.hpp"

#include "Craig_Device.hpp"

CraigError Craig::SyncManager::init(const SyncManagerInitInfo& info) {

	CraigError ret = CRAIG_SUCCESS;

	m_SM_logicalDevice = info.logicalDevice;
	m_SM_swapChainImageCount = info.swapChainImageCount;

	createSyncObjects();

	return ret;
}

void Craig::SyncManager::createSyncObjects() {

	vk::SemaphoreTypeCreateInfo typeInfo{};
	typeInfo
		.setSemaphoreType(vk::SemaphoreType::eTimeline)
		.setInitialValue(0);

	vk::SemaphoreCreateInfo timelineInfo{};
	timelineInfo.setPNext(&typeInfo);

	m_VK_timelineSemaphore = m_SM_logicalDevice.createSemaphore(timelineInfo);
	m_sempahoreTimelineValue = 0;

	mv_VK_imageAvailableSemaphores.resize(kMaxFramesInFlight);
	mv_VK_renderFinishedSemaphores.resize(m_SM_swapChainImageCount);

	vk::SemaphoreCreateInfo semaphoreInfo{};

	for (size_t i = 0; i < kMaxFramesInFlight; i++) {
		mv_VK_imageAvailableSemaphores[i] = m_SM_logicalDevice.createSemaphore(semaphoreInfo);
	}

	for (size_t i = 0; i < m_SM_swapChainImageCount; i++) {
		mv_VK_renderFinishedSemaphores[i] = m_SM_logicalDevice.createSemaphore(semaphoreInfo);
	}

}

void Craig::SyncManager::waitForGpu()
{
	if (m_sempahoreTimelineValue >= kMaxFramesInFlight) {
		uint64_t waitValue = m_sempahoreTimelineValue - (kMaxFramesInFlight - 1);

		vk::SemaphoreWaitInfo waitInfo{};
		waitInfo.setSemaphores(m_VK_timelineSemaphore);
		waitInfo.setValues(waitValue);

		// This only blocks if the GPU is really lagging
		m_SM_logicalDevice.waitSemaphores(waitInfo, UINT64_MAX);
	}
}

void Craig::SyncManager::nextFrame()
{
	m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;
}

void Craig::SyncManager::submitFrame(const std::vector<vk::CommandBuffer>& cmdBuffers, uint32_t& imageIndex, vk::Queue graphicsQueue)
{

	vk::Semaphore waitSemaphores[] = { mv_VK_imageAvailableSemaphores[m_currentFrame]};
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

	vk::Semaphore signalSemaphores[] = { m_VK_timelineSemaphore, mv_VK_renderFinishedSemaphores[imageIndex] };

	uint64_t signalValue = ++m_sempahoreTimelineValue;
	uint64_t signalValues[] = { signalValue, 0 };

	vk::TimelineSemaphoreSubmitInfo timelineSubmit;
	timelineSubmit
		.setSignalSemaphoreValueCount(2)
		.setSignalSemaphoreValues(signalValues);

	vk::SubmitInfo submitInfo;
	submitInfo
		.setPNext(&timelineSubmit)
		.setWaitSemaphoreCount(1)
		.setPWaitSemaphores(waitSemaphores)
		.setPWaitDstStageMask(waitStages)
		.setSignalSemaphoreCount(2)
		.setPSignalSemaphores(signalSemaphores)
		.setCommandBufferCount(1)
		.setPCommandBuffers(&cmdBuffers[m_currentFrame]);


	graphicsQueue.submit(
		submitInfo,
		VK_NULL_HANDLE);
}

CraigError Craig::SyncManager::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::SyncManager::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	for (size_t i = 0; i < kMaxFramesInFlight; i++) {
		m_SM_logicalDevice.destroySemaphore(mv_VK_imageAvailableSemaphores[i]);
	}

	for (size_t i = 0; i < m_SM_swapChainImageCount; i++) {
		m_SM_logicalDevice.destroySemaphore(mv_VK_renderFinishedSemaphores[i]);
	}

	m_SM_logicalDevice.destroySemaphore(m_VK_timelineSemaphore);


	return ret;
}

