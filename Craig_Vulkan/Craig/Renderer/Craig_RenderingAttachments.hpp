#pragma once
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"


namespace Craig {

	class RenderingAttachments {

	public:
		CraigError init();
		CraigError update();
		CraigError terminate();
	private:


	};



}