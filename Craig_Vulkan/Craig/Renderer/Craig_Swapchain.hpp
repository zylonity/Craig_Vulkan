#pragma once
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Craig_Window.hpp"
#include "Craig/Craig_Constants.hpp"
#include "Renderer/Craig_Device.hpp"
#include "Renderer/Craig_Image.hpp"

namespace Craig {

    class Swapchain {

    public:
        //All the stuff we need to pass to the swapchain from the renderer
        struct SwapchainInitInfo
        {
            vk::SurfaceKHR       surface;
            vk::PhysicalDevice   physicalDevice;
            vk::Device           device;
            Window*              pWindow;
        };
        // Swapchain support query results
        struct SwapChainSupportDetails {
            vk::SurfaceCapabilitiesKHR capabilities;
            std::vector<vk::SurfaceFormatKHR> formats;
            std::vector<vk::PresentModeKHR> presentModes;
        };

        CraigError init(const SwapchainInitInfo& info);
        CraigError update();
        CraigError terminate();

        void setSwapExtent();

        static const bool isSwapChainAdequate(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface);

        void createSwapChain();            // Create swapchain images
        void createSwapImageViews();           // Create swapchain image views
        void cleanupSwapChain();

        //Getters
        const vk::SwapchainKHR&           getSwapChain() const { return m_VK_swapChain; };
        const std::vector<vk::Image>&     getImages() const { return mv_VK_swapChainImages; };
        const std::vector<vk::ImageView>& getImageViews() const { return mv_VK_swapChainImageViews; };
        const vk::Format&                 getImageFormat() const { return m_VK_swapChainImageFormat; };
        const vk::Extent2D&               getExtent() const { return m_VK_swapChainExtent; };

        bool    m_vsyncEnabled = true;

    private:

        vk::SwapchainKHR           m_VK_swapChain;
        std::vector<vk::Image>     mv_VK_swapChainImages;
        std::vector<vk::ImageView> mv_VK_swapChainImageViews;
        vk::Format                 m_VK_swapChainImageFormat;
        vk::Extent2D               m_VK_swapChainExtent;

        vk::SurfaceKHR             mSC_surface;
        vk::PhysicalDevice         mSC_physicalDevice;
        vk::Device                 mSC_device;
        Window*                    mp_Window;



        static SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface);
        vk::Extent2D         chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
        vk::PresentModeKHR   chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);

    };



}