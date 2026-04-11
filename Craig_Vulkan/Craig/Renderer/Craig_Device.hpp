#pragma once
#include <optional>
#include <vulkan/vulkan.hpp>

#include "vk_mem_alloc.h"
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
		CraigError terminate();

		const vk::PhysicalDevice getPhysicalDevice() const { return m_VK_physicalDevice; }
		const vk::Device getLogicalDevice() const { return m_VK_logicalDevice; }

		const vk::Queue getGraphicsQueue() const { return m_VK_graphicsQueue; }
		const vk::Queue getPresentationQueue() const { return m_VK_presentationQueue; }
		const vk::Queue getTransferQueue() const { return m_VK_transferQueue; }

		const VmaAllocator getVmaAllocator() const { return m_VMA_allocator; }

		void createBufferVMA(vk::DeviceSize size,
			vk::BufferUsageFlags usage,
			const VmaAllocationCreateInfo& aci,
			vk::Buffer& buffer,
			VmaAllocation& alloc,
			VmaAllocationInfo* outInfo = nullptr);

	private:
		vk::SurfaceKHR m_DVC_surface;
		vk::Instance   m_DVC_instance;
		std::vector<const char*> mv_DVC_deviceExtensions;

		vk::Queue m_VK_graphicsQueue;
		vk::Queue m_VK_presentationQueue;
		vk::Queue m_VK_transferQueue;

		vk::PhysicalDevice   m_VK_physicalDevice;
		vk::Device           m_VK_logicalDevice;

		VmaAllocator m_VMA_allocator = VK_NULL_HANDLE;


		void pickPhysicalDevice(); // Choose GPU
		bool isDeviceSuitable(const vk::PhysicalDevice& device);
		bool checkDeviceExtensionSupport(const vk::PhysicalDevice& device);

		void createLogicalDevice(); // Create vk::Device + queues

		void initVMA(); // VMA allocator setup



	};

}