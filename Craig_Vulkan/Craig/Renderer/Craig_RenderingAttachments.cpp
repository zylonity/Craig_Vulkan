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
	vk::Format colourFormat = mRA_swapchain.getImageFormat();

	m_VK_colourImage = Image::createImage(mRA_physicalDevice, mRA_surface, mRA_swapchain.getFullExtent().width, mRA_swapchain.getFullExtent().height, 1, m_VK_msaaSamples, colourFormat, vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
		vk::MemoryPropertyFlagBits::eDeviceLocal,
		mRA_memoryAllocator,
		m_VMA_colourImageAllocation);

	m_VK_colourImageView = Craig::Image::createImageView(mRA_device, m_VK_colourImage, colourFormat, vk::ImageAspectFlagBits::eColor, 1);


}

CraigError Craig::RenderingAttachments::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


