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
		//Test: one triangle at z = 0, fires a ray that hits it, one that misses and one that stops before it
		CraigError runTriangleTest();
		CraigError terminate();

	private:
		//Buffers here are small so the host visible ones just stay mapped
		struct Buffer {
			vk::Buffer buffer;
			VmaAllocation allocation = VK_NULL_HANDLE;
			void* pMapped = nullptr;
		};

		struct AccelerationStructure {
			vk::AccelerationStructureKHR handle;
			Buffer storage;
			vk::DeviceAddress address = 0;
		};

		void loadEntryPoints();
		void createBuffer(Buffer& buffer, vk::DeviceSize size, vk::BufferUsageFlags usage, bool hostVisible = false);
		void uploadBuffer(Buffer& buffer, const void* pData, size_t size);
		void destroyBuffer(Buffer& buffer);
		vk::DeviceAddress getBufferAddress(const Buffer& buffer);
		void buildAccelerationStructure(vk::AccelerationStructureTypeKHR type, const VkAccelerationStructureGeometryKHR& geometry,
			AccelerationStructure& accelerationStructure, uint32_t primitiveCount);
		void destroyAccelerationStructure(AccelerationStructure& accelerationStructure);

		Device* mp_Device = nullptr;
		CommandManager* mp_CommandManager = nullptr;
		Buffer m_scratchBuffer; //Only alive while a build is happening
		vk::DeviceSize m_scratchAlignment = 1;

		//Test triangle
		Buffer m_testVertices;
		Buffer m_testIndices;
		Buffer m_testInstances;
		AccelerationStructure m_testBLAS;
		AccelerationStructure m_testTLAS;

		//The KHR acceleration structure functions aren't guaranteed to be exported by the loader, so we grab them off the device
		PFN_vkCreateAccelerationStructureKHR m_vkCreateAccelerationStructureKHR = nullptr;
		PFN_vkDestroyAccelerationStructureKHR m_vkDestroyAccelerationStructureKHR = nullptr;
		PFN_vkGetAccelerationStructureBuildSizesKHR m_vkGetAccelerationStructureBuildSizesKHR = nullptr;
		PFN_vkCmdBuildAccelerationStructuresKHR m_vkCmdBuildAccelerationStructuresKHR = nullptr;
		PFN_vkGetAccelerationStructureDeviceAddressKHR m_vkGetAccelerationStructureDeviceAddressKHR = nullptr;
	};

}
