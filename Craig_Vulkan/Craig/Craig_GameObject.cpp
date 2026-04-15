#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include "Craig_GameObject.hpp"
#include "Craig_ResourceManager.hpp"
#include "Craig_Scene.hpp"
#include "Craig_Utilities.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"

CraigError Craig::GameObject::init(std::string name, std::string modelPath, Craig::Scene* scenePtr) {

	CraigError ret = CRAIG_SUCCESS;

	m_modelPath = modelPath;
	m_name = name;
	mp_scene = scenePtr;
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

void Craig::GameObject::displayImGuiAttributes()
{

	// Show text box for the game objects name.
	std::string tempName = m_name;
	ImGui::InputText("Name", &tempName);

	if (ImGui::IsItemDeactivatedAfterEdit())
	{
		// Game object requires a name.
		if (tempName.empty())
		{
			ImGui::TextColored({ 1.0f, 0.f, 0.f, 1.0f }, "Name cannot be empty");
			tempName = m_name; // Revert
		}
		else
		{
			// Game object cannot share name with another game object in the scene.
			const GameObject* pGameObject = mp_scene->findObject(tempName);
			if (pGameObject != nullptr && pGameObject != this)
			{
				ImGui::TextColored({ 1.0f, 0.f, 0.f, 1.0f }, "Game object already exists with that name.");
				tempName = m_name; // Revert
			}
			// Name has passed validation so update it.
			else
			{
				m_name = tempName;
				// Update the game object list by sorting into alphabetical order.
				Utilities::sortGameObjectsByName(mp_scene->getGameObjects());
			}
		}
	}

	// Display transform details.
	if (ImGui::TreeNode("Transform"))
	{
		Utilities::displayVectorAttribute("Position", mv3_position);
		Utilities::displayVectorAttribute("Rotation", mv3_rotation);
		Utilities::displayVectorAttribute("Scale", mv3_scale);
		ImGui::TreePop();
	};

}

void Craig::GameObject::setPosition(glm::vec3 position)
{
	mv3_position = position;

}

void Craig::GameObject::setScale(glm::vec3 scale)
{
	mv3_scale = scale;
}



