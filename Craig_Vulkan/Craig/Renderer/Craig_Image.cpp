#include "Craig_Image.hpp"
#include "Craig_Device.hpp"

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

vk::Image Craig::Image::createImage(vk::PhysicalDevice physicalDevice, vk::SurfaceKHR surface, uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits numSamples, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, const VmaAllocator& allocator, VmaAllocation& allocation) {

    vk::ImageCreateInfo imageInfo;
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.extent.setWidth(width);
    imageInfo.extent.setHeight(height);
    imageInfo.extent.setDepth(1);
    imageInfo.setMipLevels(mipLevels);
    imageInfo.setArrayLayers(1);
    imageInfo.setFormat(format);
    imageInfo.setTiling(tiling); /*
        VK_IMAGE_TILING_LINEAR: Texels are laid out in row - major order like our pixels array
        VK_IMAGE_TILING_OPTIMAL : Texels are laid out in an implementation defined order for optimal access */
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setUsage(usage);
    imageInfo.setSamples(numSamples); //From Vulkan-Tutorial.com - The samples flag is related to multisampling. This is only relevant for images that will be used as attachments, so stick to one sample.

    Device::QueueFamilyIndices q = Device::findQueueFamilies(physicalDevice, surface);
    if (q.hasDedicatedTransfer()) {
        uint32_t families[] = { q.graphicsFamily.value(), q.transferFamily.value() };

        imageInfo.setSharingMode(vk::SharingMode::eConcurrent);
        imageInfo.setQueueFamilyIndexCount(2);
        imageInfo.setQueueFamilyIndices(families);
    }
    else {
        imageInfo.setSharingMode(vk::SharingMode::eExclusive);
    }

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;

    if (properties & vk::MemoryPropertyFlagBits::eDeviceLocal)
        aci.requiredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (properties & vk::MemoryPropertyFlagBits::eHostVisible)
        aci.requiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    VkImage tempImage;
    VkResult result = vmaCreateImage(allocator, imageInfo, &aci, &tempImage, &allocation, nullptr);

    if (result != VK_SUCCESS)
        throw std::runtime_error("vmaCreateImage failed");

    return tempImage;

}

CraigError Craig::Image::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


