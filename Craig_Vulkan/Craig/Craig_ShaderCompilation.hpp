#pragma once
#include "Craig_Constants.hpp"
#include <vulkan/vulkan.hpp>
#include <string>
namespace Craig {

	class ShaderCompilation {

	public:
		vk::ShaderModule CompileHLSLToShaderModule(vk::Device device, const std::wstring& filename);
		
	};



}