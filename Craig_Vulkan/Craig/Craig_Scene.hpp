#pragma once
#include "Craig_Constants.hpp"
#include "Craig_GameObject.hpp"

#include <vector>
namespace Craig {

	class Scene {

	public:
		CraigError init();
		CraigError update(const float& deltaTime);
		CraigError terminate();

		std::vector<Craig::GameObject*>& getGameObjects() { return mpv_Gameobjects; }
	private:

		std::vector<Craig::GameObject*> mpv_Gameobjects;
	};



}