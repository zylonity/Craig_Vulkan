#pragma once

#include "Craig_Device.hpp"
#include <array>
#include <chrono>
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

		static constexpr uint32_t kMaxRayCount = 2048;
		static constexpr size_t kResultSlotCount = 3; //How many submissions can be on the GPU at once
		using Clock = std::chrono::steady_clock;

		//What the renderer hands over each frame
		struct Request {
			std::array<Ray, kMaxRayCount> rays;
			uint32_t rayCount = 1;
			uint64_t sceneGeneration = 0;
			uint64_t sourceId = 0;
			uint64_t sequenceId = 0;
			Clock::time_point inputTime; //When the positions were read, for working out how old a result is
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

		//Everything one submission needs, so nothing gets overwritten while the GPU's still using it
		struct ResultSlot {
			Buffer instances;
			Buffer scratch;
			Buffer rays;
			Buffer results;
			AccelerationStructure tlas;
			vk::CommandPool commandPool;
			vk::CommandBuffer commandBuffer;
			vk::DescriptorSet descriptorSet;
			Request request;
			uint64_t completionValue = 0; //Timeline value this slot's submission signals when it's done
			uint64_t contextVersion = 0;
			bool inFlight = false;
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
		bool collectSceneInstances(Scene& scene, std::vector<VkAccelerationStructureInstanceKHR>& instances, bool prepareGeometry);
		void createPipeline();
		void writeQueryDescriptors(const ResultSlot& slot);
		void createResultSlots(uint32_t instanceCapacity);

		Device* mp_Device = nullptr;
		CommandManager* mp_CommandManager = nullptr;
		Buffer m_scratchBuffer; //Only alive while a build is happening
		vk::DeviceSize m_scratchAlignment = 1;
		std::unordered_map<std::string, ModelGeometry> m_modelGeometry; //By model path, so instances of the same model share one BLAS
		std::vector<VkAccelerationStructureInstanceKHR> mv_instanceSnapshot;
		std::array<ResultSlot, kResultSlotCount> m_slots;
		uint32_t m_instanceCapacity = 0; //Slot TLASes are sized for this many, set when the scene's prepared
		vk::Semaphore m_VK_completionTimeline;
		uint64_t m_lastSubmissionValue = 0;

		vk::DescriptorPool m_VK_descriptorPool;
		vk::DescriptorSetLayout m_VK_descriptorSetLayout;
		vk::PipelineLayout m_VK_pipelineLayout;
		vk::Pipeline m_VK_pipeline;

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

		vk::DescriptorPool m_VK_testDescriptorPool;
		vk::DescriptorSetLayout m_VK_testDescriptorSetLayout;
		vk::DescriptorSet m_VK_testDescriptorSet;
		vk::PipelineLayout m_VK_testPipelineLayout;
		vk::Pipeline m_VK_testPipeline;

		//The KHR acceleration structure functions aren't guaranteed to be exported by the loader, so we grab them off the device
		PFN_vkCreateAccelerationStructureKHR m_vkCreateAccelerationStructureKHR = nullptr;
		PFN_vkDestroyAccelerationStructureKHR m_vkDestroyAccelerationStructureKHR = nullptr;
		PFN_vkGetAccelerationStructureBuildSizesKHR m_vkGetAccelerationStructureBuildSizesKHR = nullptr;
		PFN_vkCmdBuildAccelerationStructuresKHR m_vkCmdBuildAccelerationStructuresKHR = nullptr;
		PFN_vkGetAccelerationStructureDeviceAddressKHR m_vkGetAccelerationStructureDeviceAddressKHR = nullptr;
	};

}
