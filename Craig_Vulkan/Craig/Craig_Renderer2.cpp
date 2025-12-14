#include <iostream>

#if defined(IMGUI_ENABLED)
#include "../External/Imgui/imgui.h"   
#include "../External/Imgui/imgui_impl_vulkan.h"
#include "../External/Imgui/imgui_impl_sdl2.h"
#endif

#include "Craig_Renderer2.hpp"
#include "Craig_Window.hpp"
#include "Craig_ShaderCompilation.hpp"
#include "Craig_Editor.hpp"


CraigError Craig::Renderer2::init(Window* CurrentWindowPtr) {

	CraigError ret = CRAIG_SUCCESS;

	// Check if the current window pointer is valid
	assert(CurrentWindowPtr != nullptr && "CurrentWindowPtr is null, cannot initialize Renderer2 without a valid window pointer.");

	//Pass in the current window pointer (Done in framework)
	mp_CurrentWindow = CurrentWindowPtr; 

	assert(mp_CurrentWindow != nullptr && "mp_CurrentWindow is null, somehow didn't get passed to our member variable");


	return ret;
}

void Craig::Renderer2::initVulkan()
{
	createInstance();
	setupDebugMessenger();
}

void Craig::Renderer2::createInstance()
{
	// Use validation layers if this is a debug build
#if defined(_DEBUG)
	mv_VK_Layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

	constexpr vk::ApplicationInfo appInfo = vk::ApplicationInfo()
		.setPApplicationName(kVK_AppName)
		.setApplicationVersion(kVK_AppVersion)
		.setPEngineName(kVK_EngineName)
		.setEngineVersion(kVK_EngineVersion)
		.setApiVersion(VK_API_VERSION_1_4);

	vk::InstanceCreateInfo createInfo = vk::InstanceCreateInfo()
#if defined(_WIN32)
		.setFlags(vk::InstanceCreateFlags())
#elif defined(__APPLE__)
		.setFlags(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR)
#endif
		.setPApplicationInfo(&appInfo)
		.setEnabledExtensionCount(static_cast<uint32_t>(mp_CurrentWindow->getExtensionsVector().size()))
		.setPpEnabledExtensionNames(mp_CurrentWindow->getExtensionsVector().data())
		.setEnabledLayerCount(static_cast<uint32_t>(mv_VK_Layers.size()))
		.setPpEnabledLayerNames(mv_VK_Layers.data());

	m_VK_instance = vk::raii::Instance(m_VK_context, createInfo);
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL Craig::Renderer2::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
	if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
	{
		std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
	}

	return vk::False;
}

void Craig::Renderer2::setupDebugMessenger() {
	vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
	vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
	vk::DebugUtilsMessengerCreateInfoEXT  debugUtilsMessengerCreateInfoEXT;
	debugUtilsMessengerCreateInfoEXT
		.setMessageSeverity(severityFlags)
		.setMessageType(messageTypeFlags)
		.setPfnUserCallback(&debugCallback);
	
	m_VK_debugMessenger = m_VK_instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
}

void Craig::Renderer2::pickPhysicalDevice() {


	auto devices = m_VK_instance.enumeratePhysicalDevices();
	if (devices.empty()) {
		throw std::runtime_error("No Vulkan-compatible GPUs found.");
	}

	for (const auto& device : devices) {
		if (isDeviceSuitable(device)) {
			m_VK_physicalDevice = device;
			break;
		}
	}

	if (m_VK_physicalDevice) {
		vk::PhysicalDeviceProperties props = m_VK_physicalDevice.getProperties();
		printf("\nFound GPU: %s\n", props.deviceName);
	}
	else {
		throw std::runtime_error("failed to find a suitable GPU!");
	}


}

CraigError Craig::Renderer2::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;



	return ret;
}

CraigError Craig::Renderer2::terminate() {

	CraigError ret = CRAIG_SUCCESS;



	return ret;
}
