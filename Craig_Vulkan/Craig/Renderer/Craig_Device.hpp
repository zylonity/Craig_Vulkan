#pragma once
#include <optional>
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"


namespace Craig {

	class Device {

	public:

		//All the stuff we need to pass to the swapchain from the renderer
		struct DeviceInitInfo
		{
			vk::SurfaceKHR       surface;
			vk::Instance        instance;
			std::vector<const char*> deviceExtensionsVector;
		};

		// Queue family indices (graphics/present/transfer)
		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
			std::optional<uint32_t> transferFamily;

			bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
			bool hasDedicatedTransfer() { return graphicsFamily && transferFamily && transferFamily != graphicsFamily; }
		};

		static QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface);

		CraigError init(DeviceInitInfo& initInfo);
		CraigError update();
		CraigError terminate();

		vk::PhysicalDevice   m_VK_physicalDevice;
		vk::Device           m_VK_logicalDevice;

		vk::Queue m_VK_graphicsQueue;
		vk::Queue m_VK_presentationQueue;
		vk::Queue m_VK_transferQueue;

	private:
		vk::SurfaceKHR m_DVC_surface;
		vk::Instance   m_DVC_instance;
		std::vector<const char*> mv_DVC_deviceExtensions;

		void pickPhysicalDevice();
		bool isDeviceSuitable(const vk::PhysicalDevice& device);
		bool checkDeviceExtensionSupport(const vk::PhysicalDevice& device);

		void createLogicalDevice();


	};



}