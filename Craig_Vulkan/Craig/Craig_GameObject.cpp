#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include "Craig_GameObject.hpp"
#include "Craig_ResourceManager.hpp"

CraigError Craig::GameObject::init() {

	CraigError ret = CRAIG_SUCCESS;
	
	Craig::ResourceManager::getInstance().loadModel(m_modelPath);

	mv3_position = { 0.0f, 0.0f, 0.0f };
	mv3_rotation = { 0.0f, 180.0f, 0.0f };

	return ret;
}

CraigError Craig::GameObject::update() {

	CraigError ret = CRAIG_SUCCESS;

	updateModelMatrix();

	return ret;
}


void Craig::GameObject::updateModelMatrix()
{
	//Empty matrix to use in order, SRT - Scale rotation Translation
	m_modelMatrix = glm::mat4(1);

	glm::vec3 mv3_rotationInRads = {
		glm::radians(mv3_rotation[0]),
		glm::radians(mv3_rotation[1]),
		glm::radians(mv3_rotation[2])
	};


	m_modelMatrix = glm::translate(m_modelMatrix, mv3_position);
	m_modelMatrix = m_modelMatrix * glm::yawPitchRoll(
		mv3_rotationInRads.y,
		mv3_rotationInRads.x,
		mv3_rotationInRads.z
	);
	m_modelMatrix = glm::scale(m_modelMatrix, mv3_scale);

}


CraigError Craig::GameObject::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}



