#pragma once
#include "Craig_Constants.hpp"
#include "Craig_Scene.hpp"

namespace Craig {
	class PhysicsEngine;

	class SceneManager {

	public:
		CraigError init(Craig::PhysicsEngine* pPhysicsEngine);
		CraigError update(const float& deltaTime); // engine side, every frame
		CraigError gameplayUpdate(const float& deltaTime); // only while the game's running
		CraigError terminate();

		CraigError loadScene(const std::string& scenePath);
		// rebuilds a scene from json in memory
		// keeps scenePath so saving still works
		CraigError loadSceneFromJson(const nlohmann::json& sceneJson, const std::string& scenePath);

		Craig::Scene* getCurrentScene() { return mp_CurrentScene; };
	private:
		// pNewScene's already been init'd
		// cleans it up if that failed, swaps it in if not
		CraigError swapInScene(Craig::Scene* pNewScene, CraigError initResult, const std::string& scenePath);

		Craig::Scene* mp_CurrentScene = nullptr;
		Craig::PhysicsEngine* mp_physicsEngine = nullptr; // not owned, the framework owns it


	};



}