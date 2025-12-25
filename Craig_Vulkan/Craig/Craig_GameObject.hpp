#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>

#include <string>

#include "Craig_Constants.hpp"


namespace Craig {

	class GameObject {

	public:
		CraigError init();
		CraigError update();
		CraigError terminate();

		std::string& getModelPath() { return m_modelPath; }
	private:
		glm::vec3 mv3_position{};
		glm::vec3 mv3_rotation{};
		glm::vec3 mv3_scale = glm::vec3(1);

		glm::mat4 m_modelMatrix{};
		glm::mat4 m_inverseModelMatrix{};

		std::string m_modelPath = "data/models/BarramundiFish.glb";

	};



}