#include "Craig_Swapchain.hpp"
#include <SDL_vulkan.h>

CraigError Craig::Swapchain::init(const SwapchainInitInfo& info) {

    CraigError ret = CRAIG_SUCCESS;

    mSC_surface = info.surface;
    mSC_physicalDevice = info.physicalDevice;
    mSC_device = info.device;
    mp_Window = info.pWindow;

    return ret;
}

CraigError Craig::Swapchain::update() {

    CraigError ret = CRAIG_SUCCESS;

    return ret;
}


void Craig::Swapchain::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(mSC_physicalDevice, mSC_surface);

    vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    m_VK_swapChainExtent = chooseSwapExtent(swapChainSupport.capabilities);

    if (swapChainSupport.capabilities.minImageCount > kMaxFramesInFlight) {
        assert("too many frames in flight for this system!");
    }

    uint32_t imageCount = kMaxFramesInFlight;

    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    printf("Creating draw buffer/swap chain with %i images\n", imageCount);
    printf("Current extent size = %i x %i\n", m_VK_swapChainExtent.width, m_VK_swapChainExtent.height);

    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo
        .setSurface(mSC_surface)
        .setMinImageCount(imageCount)
        .setImageFormat(surfaceFormat.format)
        .setImageColorSpace(surfaceFormat.colorSpace)
        .setImageExtent(m_VK_swapChainExtent)
        .setImageArrayLayers(1) //"always 1 unless you are developing a stereoscopic 3D application"
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

    Craig::Device::QueueFamilyIndices indices = Craig::Device::findQueueFamilies(mSC_physicalDevice, mSC_surface);

    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    //In case we have separate graphics and presentation queues.
    /*According to vulkan-tutorial.com
    VK_SHARING_MODE_EXCLUSIVE: An image is owned by one queue family at a time and ownership must be explicitly transferred before using it in another queue family.
    VK_SHARING_MODE_CONCURRENT: Images can be used across multiple queue families without explicit ownership transfers.
    */
    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo
            .setImageSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndexCount(2)
            .setPQueueFamilyIndices(queueFamilyIndices);
    }
    else {
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    //We can transform the image here (like a 90 degree clockwise rotation or horizontal flip.)
    //Since we don't want any, we just specify the current transformation applied (which should be none)
    createInfo
        .setPreTransform(swapChainSupport.capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque) //Blending with other windows in the window system (will mostly always be opaque)
        .setPresentMode(presentMode)
        .setClipped(VK_TRUE) //The GPU won't render pixels that are obscured (by other windows, for example)  it also means we can't trust the data in the pixels since they might've not rendered.
        .setOldSwapchain(VK_NULL_HANDLE);


    try {
        m_VK_swapChain = mSC_device.createSwapchainKHR(createInfo);
    }
    catch (const vk::SystemError& err) {
        throw std::runtime_error("failed to create swap chain!");
    }

    mv_VK_swapChainImages = mSC_device.getSwapchainImagesKHR(m_VK_swapChain);
    m_VK_swapChainImageFormat = surfaceFormat.format;
    //m_VK_swapChainExtent = m_VK_swapChainExtent;

}

Craig::Swapchain::SwapChainSupportDetails Craig::Swapchain::querySwapChainSupport(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface) {
    Swapchain::SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.presentModes = device.getSurfacePresentModesKHR(surface);


    return details;
}

vk::SurfaceFormatKHR Craig::Swapchain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {

    //Check the colour format and colour space are correct
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) { //TODO: Maybe add hdr?
            return availableFormat;
        }
    }

    return availableFormats[0];
}

vk::PresentModeKHR Craig::Swapchain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {


    //VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are transferred to the screen right away, which may result in tearing.
    //VK_PRESENT_MODE_FIFO_KHR : The swap chain is a queue where the display takes an image from the front of the queue when the display is refreshed and the program inserts rendered images at the back of the queue.If the queue is full then the program has to wait.This is most similar to vertical sync as found in modern games.The moment that the display is refreshed is known as "vertical blank".
    //VK_PRESENT_MODE_FIFO_RELAXED_KHR : This mode only differs from the previous one if the application is late and the queue was empty at the last vertical blank.Instead of waiting for the next vertical blank, the image is transferred right away when it finally arrives.This may result in visible tearing.
    //VK_PRESENT_MODE_MAILBOX_KHR : This is another variation of the second mode.Instead of blocking the application when the queue is full, the images that are already queued are simply replaced with the newer ones.This mode can be used to render frames as fast as possible while still avoiding tearing, resulting in fewer latency issues than standard vertical sync.This is commonly known as "triple buffering", although the existence of three buffers alone does not necessarily mean that the framerate is unlocked.

    // for (const auto& availablePresentMode : availablePresentModes) {
    //     vk::PresentModeKHR modeToUse = m_vsync ? vk::PresentModeKHR::eFifo : vk::PresentModeKHR::eImmediate;
    //     if (availablePresentMode == modeToUse) {
    //         return availablePresentMode;
    //     }
    // }


    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Craig::Swapchain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    else {
        //Rendering resolution, basically.
        int width, height;
        SDL_Vulkan_GetDrawableSize(mp_Window->getSDLWindow(), &width, &height);

        vk::Extent2D actualExtent;
        actualExtent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }


}

void Craig::Swapchain::createImageViews() {


    mv_VK_swapChainImageViews.resize(mv_VK_swapChainImages.size());

    for (size_t i = 0; i < mv_VK_swapChainImages.size(); i++)
    {
        mv_VK_swapChainImageViews[i] = Craig::Image::createImageView(mSC_device, mv_VK_swapChainImages[i], m_VK_swapChainImageFormat, vk::ImageAspectFlagBits::eColor, 1);

    }

}

CraigError Craig::Swapchain::terminate() {

    CraigError ret = CRAIG_SUCCESS;

    for (auto imageView : mv_VK_swapChainImageViews) {
        mSC_device.destroyImageView(imageView);
    }

    if (m_VK_swapChain)
    {
        mSC_device.destroySwapchainKHR(m_VK_swapChain);
    }



    return ret;
}


