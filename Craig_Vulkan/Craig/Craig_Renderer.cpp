#include <cassert>


#include "Craig_Renderer.hpp"
#include "Craig_Window.hpp"

CraigError Craig::Renderer::init(Window* CurrentWindowPtr) {

	CraigError ret = CRAIG_SUCCESS;

	// Check if the current window pointer is valid
	assert(CurrentWindowPtr != nullptr && "CurrentWindowPtr is null, cannot initialize Renderer without a valid window pointer.");

	//Pass in the current window pointer (Done in framework)
	mp_CurrentWindow = CurrentWindowPtr; 

	// Ensure that the current window pointer is not null (just to be extra safe)
	assert(mp_CurrentWindow != nullptr && "mp_CurrentWindow is null, somehow didn't get passed to our member variable");

	// Use validation layers if this is a debug build
#if defined(_DEBUG)
	m_VK_Layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

    // vk::ApplicationInfo allows the programmer to specifiy some basic information about the
    // program, which can be useful for layers and tools to provide more debug information.
    m_VK_appInfo = vk::ApplicationInfo()
        .setPApplicationName(kVK_AppName)
        .setApplicationVersion(kVK_AppVersion)
        .setPEngineName(kVK_EngineName)
        .setEngineVersion(kVK_EngineVersion)
        .setApiVersion(VK_API_VERSION_1_0);

    // vk::InstanceCreateInfo is where the programmer specifies the layers and/or extensions that
    // are needed.
    m_VK_instInfo = vk::InstanceCreateInfo()
        .setFlags(vk::InstanceCreateFlags())
        .setPApplicationInfo(&m_VK_appInfo)
        .setEnabledExtensionCount(static_cast<uint32_t>(mp_CurrentWindow->getExtensionsVector().size()))
        .setPpEnabledExtensionNames(mp_CurrentWindow->getExtensionsVector().data())
        .setEnabledLayerCount(static_cast<uint32_t>(m_VK_Layers.size()))
        .setPpEnabledLayerNames(m_VK_Layers.data());

	//Create the Vulkan instance
    try {
        m_VK_instance = vk::createInstance(m_VK_instInfo);
    }
    catch (const std::exception& e) {
        std::string message = std::string("Vulkan instance creation failed: ") + e.what();
		assert(false && message.c_str()); // Technicallllyyy.. undefined behavior cos of the string, but we want to crash here anyways.
    }

    // Create a Vulkan surface for rendering
    VkSurfaceKHR cSurface; // Vulkan surface for rendering
	bool sdlRetBool = SDL_Vulkan_CreateSurface(mp_CurrentWindow->getSDLWindow(), static_cast<VkInstance>(m_VK_instance), &cSurface);
    assert(sdlRetBool && "Could not create a Vulkan surface.");

    m_VK_surface = vk::SurfaceKHR(cSurface);

	return ret;
}

CraigError Craig::Renderer::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::Renderer::terminate() {

	CraigError ret = CRAIG_SUCCESS;

    // Clean up.
    m_VK_instance.destroySurfaceKHR(m_VK_surface);
    m_VK_instance.destroy();

	return ret;
}


