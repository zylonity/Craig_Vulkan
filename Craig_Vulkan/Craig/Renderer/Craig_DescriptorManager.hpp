#pragma once
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"

namespace Craig {

	class DescriptorManager {
	public:
		//All the stuff we need to pass to the RenderingAttachments from the renderer
		struct DescriptorManagerInitInfo
		{

		};


		CraigError init(const DescriptorManagerInitInfo& info);
		CraigError update();
		CraigError terminate();


	private:


	};



}