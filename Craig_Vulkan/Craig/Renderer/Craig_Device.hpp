#pragma once
#include <optional>
#include <vulkan/vulkan.hpp>

#include <vk_mem_alloc.h>
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

		//Everything sound propagation needs to fire rays with ray queries
		struct RayQuerySupport {
			bool accelerationStructureExtension = false;
			bool rayQueryExtension = false;
			bool deferredHostOperationsExtension = false;
			bool bufferDeviceAddress = false;
			bool accelerationStructure = false;
			bool rayQuery = false;
			bool graphicsQueueCompute = false; //We run the rays on the graphics queue for now so it needs compute

			bool isSupported() const {
				return accelerationStructureExtension && rayQueryExtension && deferredHostOperationsExtension &&
					bufferDeviceAddress && accelerationStructure && rayQuery && graphicsQueueCompute;
			}
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
		const QueueFamilyIndices& getQueueFamilyIndices() const { return m_DVC_queueFamilyIndices; }
		const RayQuerySupport& getRayQuerySupport() const { return m_DVC_rayQuerySupport; }
		bool isRayQueryEnabled() const { return m_DVC_rayQueryEnabled; }

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
		QueueFamilyIndices m_DVC_queueFamilyIndices; //Found once in init
		RayQuerySupport m_DVC_rayQuerySupport;
		bool m_DVC_rayQueryEnabled = false; //Only true if the device was actually created with them

		vk::Queue m_VK_graphicsQueue;
		vk::Queue m_VK_presentationQueue;
		vk::Queue m_VK_transferQueue;

		vk::PhysicalDevice   m_VK_physicalDevice;
		vk::Device           m_VK_logicalDevice;

		VmaAllocator m_VMA_allocator = VK_NULL_HANDLE;


		void pickPhysicalDevice(); // Choose GPU
		bool isDeviceSuitable(const vk::PhysicalDevice& device);
		bool checkDeviceExtensionSupport(const vk::PhysicalDevice& device);

		void queryRayQuerySupport(); // Checks what the GPU has for ray queries, doesn't turn anything on
		void createLogicalDevice(); // Create vk::Device + queues

		void initVMA(); // VMA allocator setup



	};

}