#pragma once
#include "Craig_Constants.hpp"
#include "Craig_Scene.hpp"

namespace Craig {
	class PhysicsEngine;

	class SceneManager {

	public:
		CraigError init(Craig::PhysicsEngine* pPhysicsEngine);
		CraigError update(const float& deltaTime);
		CraigError terminate();

		CraigError loadScene(const std::string& scenePath);

		Craig::Scene* getCurrentScene() { return mp_CurrentScene; };
	private:
		Craig::Scene* mp_CurrentScene = nullptr;
		Craig::PhysicsEngine* mp_physicsEngine = nullptr; // not owned, the framework owns it


	};



}