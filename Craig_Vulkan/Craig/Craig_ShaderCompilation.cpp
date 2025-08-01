#include "Craig_ShaderCompilation.hpp"

#include <iostream>
#include <atlbase.h>        // CComPtr
#include <dxc/dxcapi.h>     // DXC API



//refer to https://docs.vulkan.org/guide/latest/hlsl.html for why i wrote this
vk::ShaderModule Craig::ShaderCompilation::CompileHLSLToShaderModule(vk::Device device, const std::wstring& filename) {
	HRESULT hres;

	// Initialize DXC library
	CComPtr<IDxcLibrary> library;
	hres = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
	if (FAILED(hres)) {
		throw std::runtime_error("Could not init DXC Library");
	}

	// Initialize DXC compiler
	CComPtr<IDxcCompiler3> compiler;
	hres = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	if (FAILED(hres)) {
		throw std::runtime_error("Could not init DXC Compiler");
	}

	// Initialize DXC utility
	CComPtr<IDxcUtils> utils;
	hres = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
	if (FAILED(hres)) {
		throw std::runtime_error("Could not init DXC Utiliy");
	}

	// Load the HLSL text shader from disk
	uint32_t codePage = DXC_CP_ACP;
	CComPtr<IDxcBlobEncoding> sourceBlob;
	hres = utils->LoadFile(filename.c_str(), &codePage, &sourceBlob);
	if (FAILED(hres)) {
		throw std::runtime_error("Could not load shader file");
	}

	// Select target profile based on shader file extension
	LPCWSTR targetProfile{};
	size_t idx = filename.rfind('.');
	if (idx != std::string::npos) {
		std::wstring extension = filename.substr(idx + 1);
		if (extension == L"vert") {
			targetProfile = L"vs_6_4";
		}
		if (extension == L"frag") {
			targetProfile = L"ps_6_4";
		}
		// Mapping for other file types go here (cs_x_y, lib_x_y, etc.)
	}

	// Configure the compiler arguments for compiling the HLSL shader to SPIR-V
	std::vector<LPCWSTR> arguments = {
		// (Optional) name of the shader file to be displayed e.g. in an error message
		filename.c_str(),
		// Shader main entry point
		L"-E", L"main",
		// Shader target profile
		L"-T", targetProfile,
		// Compile to SPIRV
		L"-spirv"
	};

	// Compile shader
	DxcBuffer buffer{};
	buffer.Encoding = DXC_CP_ACP;
	buffer.Ptr = sourceBlob->GetBufferPointer();
	buffer.Size = sourceBlob->GetBufferSize();

	CComPtr<IDxcResult> result{ nullptr };
	hres = compiler->Compile(
		&buffer,
		arguments.data(),
		(uint32_t)arguments.size(),
		nullptr,
		IID_PPV_ARGS(&result));

	if (SUCCEEDED(hres)) {
		result->GetStatus(&hres);
	}

	// Output error if compilation failed
	if (FAILED(hres) && (result)) {
		CComPtr<IDxcBlobEncoding> errorBlob;
		hres = result->GetErrorBuffer(&errorBlob);
		if (SUCCEEDED(hres) && errorBlob) {
			std::cerr << "Shader compilation failed :\n\n" << (const char*)errorBlob->GetBufferPointer();
			throw std::runtime_error("Compilation failed");
		}
	}

	// Get compilation result
	CComPtr<IDxcBlob> code;
	result->GetResult(&code);

	// Create a Vulkan shader module from the compilation result
	vk::ShaderModuleCreateInfo shaderModuleCI = {
	{},
	code->GetBufferSize(),
	reinterpret_cast<const uint32_t*>(code->GetBufferPointer())
	};

	vk::ShaderModule shaderModule = device.createShaderModule(shaderModuleCI);

	return shaderModule;
}

