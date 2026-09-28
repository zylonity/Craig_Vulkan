#pragma once
#include "Craig_Constants.hpp"
#include "Craig_Scene.hpp"

namespace Craig {

	class SceneManager {

	public:
		CraigError init();
		CraigError update(const float& deltaTime);
		CraigError terminate();

		CraigError loadScene(const std::string& scenePath);

		Craig::Scene* getCurrentScene() { return mp_CurrentScene; };
	private:
		Craig::Scene* mp_CurrentScene = nullptr;


	};



}