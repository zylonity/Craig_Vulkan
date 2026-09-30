#include "Craig_ShaderCompilation.hpp"
#include "Craig_Logger.hpp"
#include <fstream>
#include <vector>

vk::ShaderModule Craig::ShaderCompilation::LoadShaderModule(vk::Device device, const std::string& filename) {

	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		// Usually means glslc didn't run, or you're running from the wrong folder (it wants to be in Craig_Vulkan/)
		Craig::Logger::renderer().critical("Couldn't open shader {}, did the shaders build and is the working directory right?", filename);
		throw std::runtime_error("failed to open shader file: " + filename);
	}

	size_t fileSize = (size_t) file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	vk::ShaderModuleCreateInfo ci{};
	ci.setCodeSize(buffer.size())
	  .setPCode(reinterpret_cast<const uint32_t*>(buffer.data()));

	Craig::Logger::renderer().debug("Loaded shader {} ({} bytes)", filename, fileSize);

	return device.createShaderModule(ci);
}
