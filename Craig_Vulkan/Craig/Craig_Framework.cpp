
//System includes
#include <cassert>

//Craig includes
#include "Craig_Framework.hpp"
#include "Craig_Window.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_ResourceManager.hpp"
#include "Craig_Editor.hpp"
#include "Craig_SceneManager.hpp"

#include <chrono>

CraigError Craig::Framework::init() {

	CraigError ret = CRAIG_SUCCESS;

	//Create our objects and get the pointers we need to initialise later
	mp_Window = new Craig::Window;
	mp_SceneManager = new Craig::SceneManager;
	mp_Renderer = new Craig::Renderer;

	//Check they were succesfully allocated just in case
	assert(mp_Window != nullptr && "mp_Window failed to allocate memory");
	assert(mp_SceneManager != nullptr && "mp_SceneManager failed to allocate memory");
	assert(mp_Renderer != nullptr && "mp_Renderer failed to allocate memory");

	//Initialise the objects
	ret = mp_Window->init();
	assert(ret == CRAIG_SUCCESS);

	Craig::ImguiEditor::getInstance().setRenderer(mp_Renderer);
	Craig::ResourceManager::getInstance().init(mp_Renderer); // Initialize the Resource Manager Singleton, this needs to be done before the renderer

	ret = mp_Renderer->init(mp_Window, mp_SceneManager);
	assert(ret == CRAIG_SUCCESS);
									

	


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

	ret = mp_SceneManager->update(elapsed);
	assert(ret == CRAIG_SUCCESS && "mp_SceneManager failed to update");

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

	ret = mp_SceneManager->terminate();
	assert(ret == CRAIG_SUCCESS && "mp_SceneManager didn't terminate properly");
	delete mp_SceneManager;
	mp_SceneManager = nullptr;

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