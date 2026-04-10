#pragma once
#include <vulkan/vulkan.hpp>


#include "Craig/Craig_Constants.hpp"
#include "Craig_ShaderCompilation.hpp"
#include "../Craig_ResourceManager.hpp"


namespace Craig {

	class Pipeline {
	public:
		//All the stuff we need to pass to the RenderingAttachments from the renderer
		struct PipelineInitInfo
		{
			vk::Device     device;
			vk::Format		colorFormat;
			vk::Format		depthFormat;
			vk::SampleCountFlagBits msaaSamples;

		};



		CraigError init(const PipelineInitInfo& info);
		CraigError update();
		CraigError terminate();

		void recreate();

		const vk::Pipeline getGraphicsPipeline() const { return m_VK_graphicsPipeline; }
		const vk::DescriptorSetLayout getDescriptorSetLayout() const { return m_VK_descriptorSetLayout; }
		const vk::PipelineLayout getPipelineLayout() const { return m_VK_pipelineLayout; }

	private:
		// Shaders / pipeline
		vk::ShaderModule       m_VK_vertShaderModule;
		vk::ShaderModule       m_VK_fragShaderModule;

		vk::DescriptorSetLayout m_VK_descriptorSetLayout;
		vk::PipelineLayout      m_VK_pipelineLayout;
		vk::Pipeline            m_VK_graphicsPipeline;

		vk::Device		mPipe_device;
		vk::Format		mPipe_colorFormat;
		vk::Format		mPipe_depthFormat;
		vk::SampleCountFlagBits mPipe_msaaSamples;

		void createGraphicsPipeline();
		void cleanupGraphicsPipeline();
		void createDescriptorSetLayout();

	};



}