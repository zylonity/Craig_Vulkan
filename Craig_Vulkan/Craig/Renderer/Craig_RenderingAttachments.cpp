#include "Craig_RenderingAttachments.hpp"
#include "Craig_Image.hpp"

CraigError Craig::RenderingAttachments::init(const RenderingAttachmentsInitInfo& info) {

	CraigError ret = CRAIG_SUCCESS;

	mRA_surface = info.surface;
	mRA_physicalDevice = info.physicalDevice;
	mRA_device = info.device;
	mRA_swapchain = info.swapchain;
	mRA_memoryAllocator = info.memoryAllocator;

	return ret;
}

CraigError Craig::RenderingAttachments::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

void Craig::RenderingAttachments::createColourResources() {
	vk::Format colourFormat = mRA_swapchain->getImageFormat();

	m_VK_colourImage = Image::createImage(mRA_physicalDevice, mRA_surface, mRA_swapchain->getExtent().width, mRA_swapchain->getExtent().height, 1, m_VK_msaaSamples, colourFormat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		mRA_memoryAllocator,
		m_VMA_colourImageAllocation);

	m_VK_colourImageView = Craig::Image::createImageView(mRA_device, m_VK_colourImage, colourFormat, vk::ImageAspectFlagBits::eColor, 1);


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

void Craig::RenderingAttachments::findAndSetMaxSampleCount(vk::PhysicalDevice physicalDevice)
{
	m_VK_msaaSamples = getMaxUsableSampleCount(physicalDevice);

}

void Craig::RenderingAttachments::createDepthResources() {

	vk::Format depthFormat = findDepthFormat();

	m_VK_depthImage = Image::createImage(mRA_physicalDevice, mRA_surface, mRA_swapchain->getExtent().width, mRA_swapchain->getExtent().height, 1, m_VK_msaaSamples, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, mRA_memoryAllocator,m_VMA_depthImageAllocation);

	m_VK_depthImageView = Craig::Image::createImageView(mRA_device,m_VK_depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);

}

vk::SampleCountFlagBits Craig::RenderingAttachments::getMaxUsableSampleCount(vk::PhysicalDevice physicalDevice) {
	vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();

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

CraigError Craig::RenderingAttachments::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


