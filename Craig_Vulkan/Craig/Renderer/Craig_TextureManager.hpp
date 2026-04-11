#pragma once
#include <vulkan/vulkan.hpp>

#include "Craig/Craig_Constants.hpp"

namespace Craig {
	class TextureManager {
	public:
		struct TextureManagerInitInfo
		{

		};

		CraigError init(const TextureManagerInitInfo& info);
		CraigError update();
		CraigError terminate();


	private:

	};



}