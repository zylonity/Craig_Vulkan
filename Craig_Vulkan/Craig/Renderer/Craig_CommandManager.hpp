#pragma once
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"

namespace Craig {
	class Device;

	class CommandManager {
	public:
		//All the stuff we need to pass to the RenderingAttachments from the renderer
		struct CommandManagerInitInfo
		{
			Craig::Device* p_Device = nullptr;
			vk::SurfaceKHR surface;

		};

		CraigError init(const CommandManagerInitInfo& info);
		CraigError update();
		CraigError terminate();

		// One-off command helpers (transfer/GFX)
		vk::CommandBuffer buffer_beginSingleTimeCommands();
		void buffer_endSingleTimeCommands(vk::CommandBuffer commandBuffer);
		vk::CommandBuffer buffer_beginSingleTimeCommandsGFX();     // Uses graphics queue
		void buffer_endSingleTimeCommandsGFX(vk::CommandBuffer commandBuffer);

		void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

		const std::vector<vk::CommandBuffer>& getCommandBuffers() { return mv_VK_commandBuffers; }

	private:

		void createCommandPool();
		void createCommandBuffers();

		// Commands
		vk::CommandPool                m_VK_commandPool;
		vk::CommandPool                m_VK_transferCommandPool;
		std::vector<vk::CommandBuffer> mv_VK_commandBuffers;

		Craig::Device* mp_Device = nullptr;
		vk::SurfaceKHR m_CM_surface;

	};



}