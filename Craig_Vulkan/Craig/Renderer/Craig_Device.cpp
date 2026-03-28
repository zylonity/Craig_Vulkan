#include "Craig_Device.hpp"

CraigError Craig::Device::init() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Device::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

//From the tutorial:
/* It has been briefly touched upon before that almost every operation in Vulkan, anything from drawing to uploading textures,
requires commands to be submitted to a queue. There are different types of queues that originate from different queue families
and each family of queues allows only a subset of commands. For example, there could be a queue family that only allows processing
of compute commands or one that only allows memory transfer related commands.*/

Craig::Device::QueueFamilyIndices Craig::Device::findQueueFamilies(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface) {
    Craig::Device::QueueFamilyIndices indices;
    // Logic to find queue family indices to populate struct with

    auto queueFamilies = device.getQueueFamilyProperties();


	//Find at least one queue family that supports graphics operations
    uint32_t i = 0;
    for (const auto& queueFamily : queueFamilies) {
        //Skip if the queuecount is 0
        if (queueFamily.queueCount == 0) continue;

        //Skip if we already assigned the graphics family queue index
        if (!indices.graphicsFamily && (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)) {
            indices.graphicsFamily = i;
        }

        //Skip if we already assigned the presentation family queue index
        if(!indices.presentFamily && device.getSurfaceSupportKHR(i, surface)) {
            indices.presentFamily = i; // If the queue family supports presentation to the surface, set the present family
		}


        if ((queueFamily.queueFlags & vk::QueueFlagBits::eTransfer) &&
            !(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) &&
            !(queueFamily.queueFlags & vk::QueueFlagBits::eCompute) &&
            !indices.transferFamily) {
            indices.transferFamily = i;
        }

        if (indices.isComplete() && indices.hasDedicatedTransfer()) {
			break; // If we already found a suitable family, no need to keep searching
        }

        i++;
    }

    // Fallback: if no dedicated transfer, use graphics (it�s implicitly transfer-capable)
    if (!indices.transferFamily && indices.graphicsFamily) {
        indices.transferFamily = indices.graphicsFamily;
    }

    return indices;
}

CraigError Craig::Device::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


