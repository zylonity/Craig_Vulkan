#pragma once
#include "Craig_Constants.hpp"
#include <vulkan/vulkan.hpp>
#include <string>
namespace Craig {

	class ShaderCompilation {

	public:
		// Loads a SPIR-V file (compiled from GLSL by glslc at build time) into a shader module
		static vk::ShaderModule LoadShaderModule(vk::Device device, const std::string& filename);

	};



}
