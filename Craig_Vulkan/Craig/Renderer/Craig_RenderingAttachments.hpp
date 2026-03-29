#pragma once
#include <vulkan/vulkan.hpp>
#include "../../External/vk_mem_alloc.h"

#include "Craig/Craig_Constants.hpp"
#include "Craig_Swapchain.hpp"

namespace Craig {

	class RenderingAttachments {
	public:

		//All the stuff we need to pass to the swapchain from the renderer
		struct RenderingAttachmentsInitInfo
		{
			vk::SurfaceKHR       surface;
			vk::PhysicalDevice   physicalDevice;
			vk::Device           device;
			Swapchain*           swapchain;
			VmaAllocator		 memoryAllocator;
		};

		vk::Image      m_VK_colourImage;
		vk::ImageView  m_VK_colourImageView;
		VmaAllocation  m_VMA_colourImageAllocation;

		// MSAA / colour / depth
		vk::SampleCountFlagBits m_VK_msaaSamples = vk::SampleCountFlagBits::e1;

		vk::Image      m_VK_depthImage;
		vk::ImageView  m_VK_depthImageView;
		VmaAllocation  m_VMA_depthImageAllocation;

		CraigError init(const RenderingAttachmentsInitInfo& info);
		CraigError update();
		CraigError terminate();

		void createColourResources();
		void createDepthResources();

		vk::Format findDepthFormat();
		void findAndSetMaxSampleCount(vk::PhysicalDevice physicalDevice); //I have to pass the device as the rendering attachments object hasn't been initiated yet
		uint32_t getMaxSamplingLevel() { return static_cast<uint32_t>(m_VK_msaaSamples); };
	private:
		vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
		vk::SampleCountFlagBits getMaxUsableSampleCount(vk::PhysicalDevice physicalDevice);

		vk::SurfaceKHR       mRA_surface;
		vk::PhysicalDevice   mRA_physicalDevice;
		vk::Device           mRA_device;
		Craig::Swapchain*    mRA_swapchain;
		VmaAllocator		 mRA_memoryAllocator;

	};



}