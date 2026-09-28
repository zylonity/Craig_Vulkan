#include "Craig_ShaderCompilation.hpp"
#include <fstream>
#include <vector>

vk::ShaderModule Craig::ShaderCompilation::LoadShaderModule(vk::Device device, const std::string& filename) {

	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
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

	return device.createShaderModule(ci);
}
