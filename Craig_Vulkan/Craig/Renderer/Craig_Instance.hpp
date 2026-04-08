#pragma once
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"


namespace Craig {

	class Instance {

	public:

		CraigError init();
		CraigError update();
		CraigError terminate();

	private:


	};



}