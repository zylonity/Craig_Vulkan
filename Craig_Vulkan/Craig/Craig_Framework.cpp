
//System includes
#include <cassert>

//Craig includes
#include "Craig_Framework.hpp"
#include "Craig_Window.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_ResourceManager.hpp"

CraigError Craig::Framework::init() {

	CraigError ret = CRAIG_SUCCESS;

	// Initialize our Window
	// NEEDS to be done first so we can pass it to the Renderer WITH data
	mp_Window = new Craig::Window;
	assert(mp_Window != nullptr && "mp_Window failed to allocate memory");	// Always checking with any new if we have successfully allocated memory for it
	ret = mp_Window->init();
	assert(ret == CRAIG_SUCCESS);											// Always checking if we have a valid return code

	// Initialize our Renderer
	mp_Renderer = new Craig::Renderer;
	assert(mp_Renderer != nullptr && "mp_Renderer failed to allocate memory");
	ret = mp_Renderer->init(mp_Window);
	assert(ret == CRAIG_SUCCESS);											

	// Initialize the Resource Manager Singleton
	Craig::ResourceManager::getInstance().init();

#if defined(IMGUI_ENABLED)
	
#endif

	return ret;
}


CraigError Craig::Framework::update() {

	CraigError ret = CRAIG_SUCCESS;

	ret = mp_Window->update();
	assert((ret == CRAIG_SUCCESS || ret == CRAIG_CLOSED) && "mp_Window failed to update");
	if(ret == CRAIG_CLOSED) {
		return CRAIG_CLOSED; // If the window is closed, we return that code
	}

	ret = mp_Renderer->update();
	assert(ret == CRAIG_SUCCESS && "mp_Renderer failed to update");

	return ret;
}

CraigError Craig::Framework::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	ret = mp_Window->terminate(); //Delete left over items in memory
	assert(ret == CRAIG_SUCCESS && "mp_Window didn't terminate properly"); //Check it closed properly
	delete mp_Window; //Delete the scene manager
	mp_Window = nullptr; //Set the pointer to null (Might not be done by default, just in case)

	ret = mp_Renderer->terminate(); //Delete left over items in memory
	assert(ret == CRAIG_SUCCESS && "mp_Renderer didn't terminate properly"); //Check it closed properly
	delete mp_Renderer; //Delete the scene manager
	mp_Renderer = nullptr; //Set the pointer to null (Might not be done by default, just in case)

	Craig::ResourceManager::getInstance().terminate();

	return ret;
}