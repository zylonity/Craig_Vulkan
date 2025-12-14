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

CraigError Craig::Renderer2::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;



	return ret;
}

void Craig::Renderer2::initVulkan()
{
	createInstance();

}

void Craig::Renderer2::createInstance()
{
}

CraigError Craig::Renderer2::terminate() {

	CraigError ret = CRAIG_SUCCESS;



	return ret;
}
