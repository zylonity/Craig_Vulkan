#include "Craig_Image.hpp"
#include "Craig_Device.hpp"
#include "Craig_CommandManager.hpp"

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

//For us to copy the buffer that contains the picture data to the vulkan image, we need to make sure it's the right layout first
void Craig::Image::transitionImageLayout(Craig::CommandManager& commandManager, vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, bool useTransferQueue, uint32_t mipLevels) {
    //Begin recording to buffer
    vk::CommandBuffer tempBuffer = useTransferQueue ? commandManager.buffer_beginSingleTimeCommands() : commandManager.buffer_beginSingleTimeCommandsGFX();

    vk::ImageMemoryBarrier2 barrier{};
    barrier
        .setOldLayout(oldLayout)
        .setNewLayout(newLayout)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
        /*From vulkan-tutorial.com
        The image and subresourceRange specify the image that is affected and the specific part of the image.
        Our image is not an array and does not have mipmapping levels, so only one level and layer are specified.*/
        .setImage(image)
        .subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
                         .setBaseMipLevel(0)
                         .setLevelCount(mipLevels)
                         .setBaseArrayLayer(0)
                         .setLayerCount(1);


    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        barrier
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
            .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite);

    }
    else if(oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal){

        barrier
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderRead);
    }
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }


    vk::DependencyInfo dep{};
    dep
        .setImageMemoryBarrierCount(1)
        .setPImageMemoryBarriers(&barrier)
        .setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    tempBuffer.pipelineBarrier2(dep);


    useTransferQueue ?  commandManager.buffer_endSingleTimeCommands(tempBuffer) :  commandManager.buffer_endSingleTimeCommandsGFX(tempBuffer);

}

void Craig::Image::transitionSwapImage(vk::CommandBuffer cmd, vk::Image img, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::ImageAspectFlags aspect = (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    ? (vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil)
    : vk::ImageAspectFlagBits::eColor;


    vk::ImageMemoryBarrier2 barrier{};
    barrier
        .setOldLayout(oldLayout)
        .setNewLayout(newLayout)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setImage(img)
        .setSubresourceRange({ aspect, 0, 1, 0, 1 });


    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eColorAttachmentOptimal)
    {
        barrier
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
            .setSrcAccessMask(vk::AccessFlagBits2::eNone)
            .setDstStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
            .setDstAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite);
    }
    else if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        barrier
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
            .setSrcAccessMask({})
            .setDstStageMask(vk::PipelineStageFlagBits2::eEarlyFragmentTests)
            .setDstAccessMask(vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite);
    }
    else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal &&
        newLayout == vk::ImageLayout::ePresentSrcKHR)
    {
        barrier
            .setSrcStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput)
            .setSrcAccessMask(vk::AccessFlagBits2::eColorAttachmentWrite)
            .setDstStageMask(vk::PipelineStageFlagBits2::eBottomOfPipe)
            .setDstAccessMask(vk::AccessFlagBits2::eNone);
    }
    else
    {
        throw std::runtime_error("unsupported swapchain layout transition");
    }

    vk::DependencyInfo dep{};
    dep.setImageMemoryBarrierCount(1).setPImageMemoryBarriers(&barrier);
    cmd.pipelineBarrier2(dep);
}

void Craig::Image::copyBufferToImage(Craig::CommandManager& commandManager, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) {
    vk::CommandBuffer tempBuffer = commandManager.buffer_beginSingleTimeCommands();

    vk::BufferImageCopy region{};
    region.setBufferOffset(0)
        .setBufferRowLength(0)
        .setBufferImageHeight(0);

    region.imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setMipLevel(0)
        .setBaseArrayLayer(0)
        .setLayerCount(1);

    region.setImageOffset({ 0, 0, 0 })
        .setImageExtent({
            width,
            height,
            1
            });

    tempBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);


    commandManager.buffer_endSingleTimeCommands(tempBuffer);
}

void Craig::Image::generateMipMaps(Craig::CommandManager& commandManager, vk::FormatProperties formatProperties, vk::Image image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, bool useTransferQueue) {


    if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImageFilterLinear)) {
       assert("texture image format does not support linear blitting!");
    }

    vk::CommandBuffer tempBuffer = useTransferQueue ? commandManager.buffer_beginSingleTimeCommands() : commandManager.buffer_beginSingleTimeCommandsGFX();

    //This means we're changing an image from x state to y state, and we gotta sync the access types
    vk::ImageMemoryBarrier2 barrier{};
    barrier
        .setImage(image)
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
        .subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor)
                         .setBaseArrayLayer(0)
                         .setLayerCount(1)
                         .setLevelCount(1);

    vk::DependencyInfo dep{};
    dep.setImageMemoryBarrierCount(1)
        .setPImageMemoryBarriers(&barrier);

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    //So for every mip level we want to generate, the source mip level is 'i - 1' and the destination would be 'i'
    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.setBaseMipLevel(i - 1);
        barrier
            .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
            .setNewLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
            .setDstAccessMask(vk::AccessFlagBits2::eTransferRead);

        tempBuffer.pipelineBarrier2(dep);

        vk::ImageBlit blit;
        //SRC
        blit.srcOffsets[0].setX(0)
                          .setY(0)
                          .setZ(0);

        blit.srcOffsets[1].setX(mipWidth)
                          .setY(mipHeight)
                          .setZ(1);

        blit.srcSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor)
                           .setMipLevel(i - 1)
                           .setBaseArrayLayer(0)
                           .setLayerCount(1);

        //DST
        blit.dstOffsets[0].setX(0)
                          .setY(0)
                          .setZ(0);

        //Each mip level is half the size of the previous level
        blit.dstOffsets[1].setX(mipWidth > 1 ? mipWidth / 2 : 1)
                          .setY(mipHeight > 1 ? mipHeight / 2 : 1)
                          .setZ(1); //Has to be 1, since 2D images have a depth of 1

        blit.dstSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor)
                           .setMipLevel(i)
                           .setBaseArrayLayer(0)
                           .setLayerCount(1);

        tempBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal, image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear); //Linear filter for interpolation

        barrier
            .setOldLayout(vk::ImageLayout::eTransferSrcOptimal)
            .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
            .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
            .setSrcAccessMask(vk::AccessFlagBits2::eTransferRead)
            .setDstAccessMask(vk::AccessFlagBits2::eShaderRead);

        tempBuffer.pipelineBarrier2(dep);

        if (mipWidth > 1) { mipWidth /= 2; };
        if (mipHeight > 1) { mipHeight /= 2; };

    }

    barrier.subresourceRange.setBaseMipLevel(mipLevels - 1);
    barrier
        .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
        .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
        .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
        .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
        .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
        .setDstAccessMask(vk::AccessFlagBits2::eShaderRead);

    tempBuffer.pipelineBarrier2(dep);

    useTransferQueue ?  commandManager.buffer_endSingleTimeCommands(tempBuffer) :  commandManager.buffer_endSingleTimeCommandsGFX(tempBuffer);

}

CraigError Craig::Image::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


