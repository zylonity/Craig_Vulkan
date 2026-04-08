#pragma once
#include <vulkan/vulkan.hpp>
#include "../../External/vk_mem_alloc.h"

#include "Craig/Craig_Constants.hpp"
#include "Craig_Swapchain.hpp"

namespace Craig {

	class RenderingAttachments {
	public:

		//All the stuff we need to pass to the RenderingAttachments from the renderer
		struct RenderingAttachmentsInitInfo
		{
			vk::SurfaceKHR       surface;
			vk::PhysicalDevice   physicalDevice;
			vk::Device           device;
			VmaAllocator		 memoryAllocator;
		};

		// MSAA / colour / depth
		vk::SampleCountFlagBits m_VK_msaaSamples = vk::SampleCountFlagBits::e1;

		CraigError init(const RenderingAttachmentsInitInfo& info);
		CraigError update();
		CraigError terminate();

		void createColourResources(vk::Extent2D extent, vk::Format colourFormat);
		void createDepthResources(vk::Extent2D extent);

		//Cleanup functions
		void cleanupColourAndDepthImageViews();

		//Utility functions
		vk::Format findDepthFormat();
		//void findAndSetMaxSampleCount(vk::PhysicalDevice physicalDevice); //I have to pass the device as the rendering attachments object hasn't been initiated yet
		uint32_t getMaxSamplingLevel() { return static_cast<uint32_t>(m_VK_msaaSamples); };

		//Getters
		const vk::Image      getColourImage() const { return m_VK_colourImage; };
		const vk::ImageView  getColourImageView() const { return m_VK_colourImageView; };

		const vk::Image      getDepthImage() const { return m_VK_depthImage; };
		const vk::ImageView  getDepthImageView() const {return m_VK_depthImageView; };
	private:
		vk::Image      m_VK_colourImage;
		vk::ImageView  m_VK_colourImageView;
		VmaAllocation  m_VMA_colourImageAllocation;

		vk::Image      m_VK_depthImage;
		vk::ImageView  m_VK_depthImageView;
		VmaAllocation  m_VMA_depthImageAllocation;

		vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
		vk::SampleCountFlagBits getMaxUsableSampleCount();

		vk::SurfaceKHR       mRA_surface;
		vk::PhysicalDevice   mRA_physicalDevice;
		vk::Device           mRA_device;
		VmaAllocator		 mRA_memoryAllocator;

	};



}