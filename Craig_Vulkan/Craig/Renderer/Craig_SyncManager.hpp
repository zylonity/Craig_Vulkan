#pragma once
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"

namespace Craig {
	class Device;

	class SyncManager {
	public:
		struct SyncManagerInitInfo
		{
			vk::Device logicalDevice;
			uint32_t swapChainImageCount;

		};

		CraigError init(const SyncManagerInitInfo& info);
		CraigError terminate();

		void waitForGpu();
		void nextFrame();

		void submitFrame(const std::vector<vk::CommandBuffer>& cmdBuffers, uint32_t& imageIndex, vk::Queue graphicsQueue);

		const uint32_t getCurrentFrame() const { return m_currentFrame; }
		const std::vector<vk::Semaphore>& getVK_imageAvailableSemaphores() const { return mv_VK_imageAvailableSemaphores; }
		const std::vector<vk::Semaphore>& getVK_renderFinishedSemaphores() const { return mv_VK_renderFinishedSemaphores; }

	private:
		void createSyncObjects();

		// Sync
		uint32_t m_currentFrame = 0;

		std::vector<vk::Semaphore> mv_VK_imageAvailableSemaphores;
		std::vector<vk::Semaphore> mv_VK_renderFinishedSemaphores;

		// Timeline semaphore (optional sync style)
		vk::Semaphore m_VK_timelineSemaphore;
		uint64_t      m_sempaphoreTimelineValue = 0;

		vk::Device m_SM_logicalDevice;
		uint32_t m_SM_swapChainImageCount;

	};



}