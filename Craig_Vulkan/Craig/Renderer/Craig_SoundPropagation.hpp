#pragma once

#include "Craig_Device.hpp"
#include <array>
#include <vector>

namespace Craig {

	class CommandManager;

	//Fires rays through the scene with ray queries so we can work out how sound gets from a source to the listener
	class SoundPropagation {
	public:
		struct Ray {
			std::array<float, 4> origin; // w is tmin
			std::array<float, 4> direction; // w is tmax, xyz is normalised
		};

		CraigError init(Device* pDevice, CommandManager* pCommandManager);
		CraigError terminate();

	private:
		//Buffers here are small so the host visible ones just stay mapped
		struct Buffer {
			vk::Buffer buffer;
			VmaAllocation allocation = VK_NULL_HANDLE;
			void* pMapped = nullptr;
		};

		void loadEntryPoints();
		void createBuffer(Buffer& buffer, vk::DeviceSize size, vk::BufferUsageFlags usage, bool hostVisible = false);
		void uploadBuffer(Buffer& buffer, const void* pData, size_t size);
		void destroyBuffer(Buffer& buffer);
		vk::DeviceAddress getBufferAddress(const Buffer& buffer);

		Device* mp_Device = nullptr;
		CommandManager* mp_CommandManager = nullptr;
		vk::DeviceSize m_scratchAlignment = 1;

		//The KHR acceleration structure functions aren't guaranteed to be exported by the loader, so we grab them off the device
		PFN_vkCreateAccelerationStructureKHR m_vkCreateAccelerationStructureKHR = nullptr;
		PFN_vkDestroyAccelerationStructureKHR m_vkDestroyAccelerationStructureKHR = nullptr;
		PFN_vkGetAccelerationStructureBuildSizesKHR m_vkGetAccelerationStructureBuildSizesKHR = nullptr;
		PFN_vkCmdBuildAccelerationStructuresKHR m_vkCmdBuildAccelerationStructuresKHR = nullptr;
		PFN_vkGetAccelerationStructureDeviceAddressKHR m_vkGetAccelerationStructureDeviceAddressKHR = nullptr;
	};

}
