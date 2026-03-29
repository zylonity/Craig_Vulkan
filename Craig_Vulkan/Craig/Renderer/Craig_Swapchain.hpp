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

        SwapChainSupportDetails querySwapChainSupport(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface);
        vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
        vk::PresentModeKHR   chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
        vk::Extent2D         chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

        void createSwapChain();            // Create swapchain images
        void createImageViews();           // Create swapchain image views

        //Getters
        const vk::SwapchainKHR&           getSwapChain() const { return m_VK_swapChain; };
        const std::vector<vk::Image>&     getImages() const { return mv_VK_swapChainImages; };
        const std::vector<vk::ImageView>& getImageViews() const { return mv_VK_swapChainImageViews; };
        const vk::Format&                 getImageFormat() const { return m_VK_swapChainImageFormat; };
        vk::Extent2D&                     getExtent() { return m_VK_swapChainExtent; };

    private:

        vk::SwapchainKHR           m_VK_swapChain;
        std::vector<vk::Image>     mv_VK_swapChainImages;
        std::vector<vk::ImageView> mv_VK_swapChainImageViews;
        vk::Format                 m_VK_swapChainImageFormat;
        vk::Extent2D               m_VK_swapChainExtent;

        vk::SurfaceKHR       mSC_surface;
        vk::PhysicalDevice   mSC_physicalDevice;
        vk::Device           mSC_device;
        Window*              mp_Window;

        // Swapchain + framebuffer resources
        // void recreateSwapChain();          // Swapchain-only recreation
        // void recreateSwapChainFull();      // Swapchain + pipeline + imgui recreation


        // void cleanupSwapChain();           // Destroy swapchain-related objects


    };



}