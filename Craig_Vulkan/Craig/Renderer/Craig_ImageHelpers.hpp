#pragma once
#include <vulkan/vulkan.hpp>
#include "../../External/vk_mem_alloc.h"

#include "../Craig_Constants.hpp"


namespace Craig {
	class CommandManager;

	class ImageHelpers {

	public:

		static vk::ImageView createImageView(vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);
		static vk::Image createImage(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, const VmaAllocator& allocator, VmaAllocation& allocation);

		static void transitionImageLayout(Craig::CommandManager& commandManager, vk::Image image, vk::Format format,
			vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
			bool useTransferQueue = true, uint32_t mipLevels = 1);

		static void transitionSwapImage(vk::CommandBuffer cmd, vk::Image img,
			vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

		static void copyBufferToImage(Craig::CommandManager& commandManager, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

		static void generateMipMaps(Craig::CommandManager& commandManager, vk::FormatProperties formatProperties, vk::Image image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, bool useTransferQueue);

	private:


	};



}