#pragma once
#include "Craig_Constants.hpp"
#include "Craig_GameObject.hpp"

#include <vector>

#include "Craig_Camera.hpp"

namespace Craig {

	class Scene {

	public:
		CraigError init();
		CraigError update(const float& deltaTime);
		CraigError terminate();

		std::vector<Craig::GameObject*>& getGameObjects() { return mpv_Gameobjects; }
		Craig::GameObject* findObject(const std::string& objectName) const;

		Craig::Camera& getCamera() { return m_camera; }
	private:

		std::vector<Craig::GameObject*> mpv_Gameobjects;

		Craig::Camera m_camera = Craig::Camera(); //Virtual camera for the scene
	};



}
