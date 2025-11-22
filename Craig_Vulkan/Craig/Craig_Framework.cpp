
//System includes
#include <cassert>

//Craig includes
#include "Craig_Framework.hpp"
#include "Craig_Window.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_ResourceManager.hpp"
#include "Craig_Editor.hpp"

#include <chrono>

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
	Craig::ImguiEditor::getInstance().setRenderer(mp_Renderer);
	assert(mp_Renderer != nullptr && "mp_Renderer failed to allocate memory");
	ret = mp_Renderer->init(mp_Window);
	assert(ret == CRAIG_SUCCESS);											

	// Initialize the Resource Manager Singleton
	Craig::ResourceManager::getInstance().init();

	m_LastFrameTime = std::chrono::steady_clock::now();

	return ret;
}


CraigError Craig::Framework::update() {

	CraigError ret = CRAIG_SUCCESS;

	const float elapsed = getElapsedTime();

	ret = mp_Window->update(elapsed);
	assert((ret == CRAIG_SUCCESS || ret == CRAIG_CLOSED) && "mp_Window failed to update");
	if(ret == CRAIG_CLOSED) {
		return CRAIG_CLOSED; // If the window is closed, we return that code
	}

	ret = mp_Renderer->update(elapsed);
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

float Craig::Framework::getElapsedTime()
{
	// Calculate frame time as current time - time when we last called the function.
	const std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
	const float elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_LastFrameTime).count() / 1000000.f;

	// Update our reference to the recorded time.
	m_LastFrameTime = currentTime;

	return elapsedTime;
}