#pragma once
#include "Craig_Constants.hpp"


namespace Craig {

	class SceneManager {

	public:
		CraigError init();
		CraigError update(const float& deltaTime);
		CraigError terminate();
	private:


	};



}