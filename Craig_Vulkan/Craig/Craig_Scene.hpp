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

		std::vector<Craig::GameObject>& getGameObjects() { return mv_Gameobjects; }
	private:

		Craig::GameObject m_MainObject;

		std::vector<Craig::GameObject> mv_Gameobjects;
	};



}