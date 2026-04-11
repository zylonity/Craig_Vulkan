#include "Craig_RenderingAttachments.hpp"
#include "Craig_ImageHelpers.hpp"

CraigError Craig::RenderingAttachments::init(const RenderingAttachmentsInitInfo& info) {

	CraigError ret = CRAIG_SUCCESS;

	mRA_surface = info.surface;
	mRA_physicalDevice = info.physicalDevice;
	mRA_device = info.device;
	mRA_memoryAllocator = info.memoryAllocator;

	m_VK_msaaSamples = getMaxUsableSampleCount();

	return ret;
}

CraigError Craig::RenderingAttachments::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

void Craig::RenderingAttachments::createColourResources(vk::Extent2D extent, vk::Format colourFormat) {

	m_VK_colourImage = ImageHelpers::createImage(mRA_physicalDevice, mRA_surface, extent.width, extent.height, 1, m_VK_msaaSamples, colourFormat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		mRA_memoryAllocator,
		m_VMA_colourImageAllocation);

	m_VK_colourImageView = Craig::ImageHelpers::createImageView(mRA_device, m_VK_colourImage, colourFormat, vk::ImageAspectFlagBits::eColor, 1);


}

vk::Format Craig::RenderingAttachments::findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
{
	for (auto format : candidates) {
		auto props = mRA_physicalDevice.getFormatProperties(format);

		if (tiling == vk::ImageTiling::eLinear &&
			(props.linearTilingFeatures & features) == features)
			return format;

		if (tiling == vk::ImageTiling::eOptimal &&
			(props.optimalTilingFeatures & features) == features)
			return format;
	}

	throw std::runtime_error("no supported depth format found");
}

vk::Format Craig::RenderingAttachments::findDepthFormat() {
	return findSupportedFormat(
		{
			vk::Format::eD32SfloatS8Uint,
			vk::Format::eD24UnormS8Uint,
			vk::Format::eD32Sfloat
		},
		vk::ImageTiling::eOptimal,
		vk::FormatFeatureFlagBits::eDepthStencilAttachment
	);
}

void Craig::RenderingAttachments::createDepthResources(vk::Extent2D extent) {

	vk::Format depthFormat = findDepthFormat();

	m_VK_depthImage = ImageHelpers::createImage(mRA_physicalDevice, mRA_surface, extent.width, extent.height, 1, m_VK_msaaSamples, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, mRA_memoryAllocator,m_VMA_depthImageAllocation);

	m_VK_depthImageView = Craig::ImageHelpers::createImageView(mRA_device,m_VK_depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);

}

vk::SampleCountFlagBits Craig::RenderingAttachments::getMaxUsableSampleCount() {
	vk::PhysicalDeviceProperties physicalDeviceProperties = mRA_physicalDevice.getProperties();

	vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

	if (counts & vk::SampleCountFlagBits::e64) {
		return vk::SampleCountFlagBits::e64;
	}
	if (counts & vk::SampleCountFlagBits::e32) {
		return vk::SampleCountFlagBits::e32;
	}
	if (counts & vk::SampleCountFlagBits::e16) {
		return vk::SampleCountFlagBits::e16;
	}
	if (counts & vk::SampleCountFlagBits::e8) {
		return vk::SampleCountFlagBits::e8;
	}
	if (counts & vk::SampleCountFlagBits::e4) {
		return vk::SampleCountFlagBits::e4;
	}
	if (counts & vk::SampleCountFlagBits::e2) {
		return vk::SampleCountFlagBits::e2;
	}

	return vk::SampleCountFlagBits::e1;

}

void Craig::RenderingAttachments::cleanupColourAndDepthImageViews() {

	mRA_device.destroyImageView(m_VK_colourImageView);
	vmaDestroyImage(mRA_memoryAllocator, m_VK_colourImage, m_VMA_colourImageAllocation);

	mRA_device.destroyImageView(m_VK_depthImageView);
	vmaDestroyImage(mRA_memoryAllocator, m_VK_depthImage, m_VMA_depthImageAllocation);

}

CraigError Craig::RenderingAttachments::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	cleanupColourAndDepthImageViews();

	return ret;
}


