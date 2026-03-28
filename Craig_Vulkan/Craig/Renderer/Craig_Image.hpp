#pragma once
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"


namespace Craig {

	class Image {

	public:

		static vk::ImageView createImageView(vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels);

		CraigError init();
		CraigError update();
		CraigError terminate();
	private:


	};



}