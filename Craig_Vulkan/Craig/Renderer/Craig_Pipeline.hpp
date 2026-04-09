#pragma once
#include <vulkan/vulkan.hpp>


#include "Craig/Craig_Constants.hpp"


namespace Craig {

	class Pipeline {
	public:
		//All the stuff we need to pass to the RenderingAttachments from the renderer
		struct PipelineInitInfo
		{

		};

		CraigError init(const PipelineInitInfo& info);
		CraigError update();
		CraigError terminate();

	private:


	};



}