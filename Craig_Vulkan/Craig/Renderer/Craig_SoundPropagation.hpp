#pragma once

#include "Craig_Device.hpp"
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace Craig {

	class CommandManager;
	class Scene;
	struct Model;

	//Fires rays through the scene with ray queries so we can work out how sound gets from a source to the listener
	class SoundPropagation {
	public:
		struct Ray {
			std::array<float, 4> origin; // w is tmin
			std::array<float, 4> direction; // w is tmax, xyz is normalised
		};

		//Builds a BLAS for every model in the scene that blocks sound
		CraigError init(Device* pDevice, CommandManager* pCommandManager, Scene* pScene);
		//Test: one triangle at z = 0, fires a ray that hits it, one that misses and one that stops before it
		CraigError runTriangleTest();
		//Test: fires rays around the reference scene (Listener, Source and a wall between them) and checks what they hit
		CraigError runSceneTest(Scene& scene);
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

		//Each model gets its own copy of its triangles, separate from the raster vertex/index buffers
		struct ModelGeometry {
			Buffer vertices;
			Buffer indices;
			AccelerationStructure blas;
		};

		void loadEntryPoints();
		void createBuffer(Buffer& buffer, vk::DeviceSize size, vk::BufferUsageFlags usage, bool hostVisible = false);
		void uploadBuffer(Buffer& buffer, const void* pData, size_t size);
		void destroyBuffer(Buffer& buffer);
		vk::DeviceAddress getBufferAddress(const Buffer& buffer);
		void buildAccelerationStructure(vk::AccelerationStructureTypeKHR type, const VkAccelerationStructureGeometryKHR& geometry,
			AccelerationStructure& accelerationStructure, uint32_t primitiveCount);
		void destroyAccelerationStructure(AccelerationStructure& accelerationStructure);
		void createTestPipeline();
		//Blocking, waits for the GPU, only for the startup tests
		std::vector<uint32_t> traceTestRays(const AccelerationStructure& tlas, const std::vector<Ray>& rays);
		CraigError checkTestResults(const char* testName, const std::vector<uint32_t>& results, const std::vector<uint32_t>& expected);
		void createModelGeometry(const Model& model, ModelGeometry& geometry);
		void collectSceneInstances(Scene& scene, std::vector<VkAccelerationStructureInstanceKHR>& instances);

		Device* mp_Device = nullptr;
		CommandManager* mp_CommandManager = nullptr;
		Buffer m_scratchBuffer; //Only alive while a build is happening
		vk::DeviceSize m_scratchAlignment = 1;
		std::unordered_map<std::string, ModelGeometry> m_modelGeometry; //By model path, so instances of the same model share one BLAS
		std::vector<VkAccelerationStructureInstanceKHR> mv_instanceSnapshot;

		//Test triangle
		Buffer m_testVertices;
		Buffer m_testIndices;
		Buffer m_testInstances;
		AccelerationStructure m_testBLAS;
		AccelerationStructure m_testTLAS;
		Buffer m_sceneInstances;
		AccelerationStructure m_sceneTLAS;
		Buffer m_testRays;
		Buffer m_testResults;

		vk::DescriptorPool m_VK_descriptorPool;
		vk::DescriptorSetLayout m_VK_descriptorSetLayout;
		vk::DescriptorSet m_VK_descriptorSet;
		vk::PipelineLayout m_VK_pipelineLayout;
		vk::Pipeline m_VK_pipeline;

		//The KHR acceleration structure functions aren't guaranteed to be exported by the loader, so we grab them off the device
		PFN_vkCreateAccelerationStructureKHR m_vkCreateAccelerationStructureKHR = nullptr;
		PFN_vkDestroyAccelerationStructureKHR m_vkDestroyAccelerationStructureKHR = nullptr;
		PFN_vkGetAccelerationStructureBuildSizesKHR m_vkGetAccelerationStructureBuildSizesKHR = nullptr;
		PFN_vkCmdBuildAccelerationStructuresKHR m_vkCmdBuildAccelerationStructuresKHR = nullptr;
		PFN_vkGetAccelerationStructureDeviceAddressKHR m_vkGetAccelerationStructureDeviceAddressKHR = nullptr;
	};

}
