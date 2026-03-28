#pragma once
#include <optional>
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"


namespace Craig {

	class Device {

	public:

		// Queue family indices (graphics/present/transfer)
		struct QueueFamilyIndices {
			std::optional<uint32_t> graphicsFamily;
			std::optional<uint32_t> presentFamily;
			std::optional<uint32_t> transferFamily;

			bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
			bool hasDedicatedTransfer() { return graphicsFamily && transferFamily && transferFamily != graphicsFamily; }
		};

		static QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface);

		CraigError init();
		CraigError update();
		CraigError terminate();
	private:


	};



}