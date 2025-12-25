#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

#include "Craig_Constants.hpp"


namespace Craig {

	class GameObject {

	public:
		CraigError init();
		CraigError update();
		CraigError terminate();

		glm::mat4 GetModelMatrix() { return m_modelMatrix; }

		const std::string& getModelPath() const { return m_modelPath; }
	private:
		void updateModelMatrix();

		glm::vec3 mv3_position{};
		glm::vec3 mv3_rotation{};
		glm::vec3 mv3_scale = glm::vec3(1);

		glm::mat4 m_modelMatrix = glm::mat4(1);
		glm::mat4 m_inverseModelMatrix{};

		const std::string m_modelPath = "data/models/BarramundiFish.glb";

	};



}