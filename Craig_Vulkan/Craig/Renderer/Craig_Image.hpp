#pragma once
#include <vulkan/vulkan.hpp>
#include "../../External/vk_mem_alloc.h"

#include "Craig/Craig_Constants.hpp"


namespace Craig {

	class Image {

	public:

		static vk::ImageView createImageView(vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);
		static vk::Image createImage(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, const VmaAllocator& allocator, VmaAllocation& allocation);

		CraigError init();
		CraigError update();
		CraigError terminate();

	private:


	};



}