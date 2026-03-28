#include "Craig_Image.hpp"

CraigError Craig::Image::init() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Image::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

vk::ImageView Craig::Image::createImageView(vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags, uint32_t mipLevels) {
	vk::ImageViewCreateInfo createInfo{};
	createInfo
		.setImage(image)
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(format)
		.setComponents(vk::ComponentMapping{
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity,
			vk::ComponentSwizzle::eIdentity
			})
		.setSubresourceRange(vk::ImageSubresourceRange{
			aspectFlags,  // aspectMask
			0, mipLevels, // baseMipLevel, levelCount
			0, 1          // baseArrayLayer, layerCount
			});


	vk::ImageView imageView;

	try {
		imageView = device.createImageView(createInfo);
	}
	catch (const vk::SystemError& err) {
		throw std::runtime_error("failed to create image views!");
	}

	return imageView;
}

CraigError Craig::Image::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


