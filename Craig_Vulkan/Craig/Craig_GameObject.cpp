#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include "Craig_GameObject.hpp"
#include "Craig_Scene.hpp"
#include "Components/Craig_Model.hpp"
#include "Components/Craig_Sun.hpp"
#include "Craig_Utilities.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "Components/Craig_BoxCollider.hpp"
#include "Components/Craig_SphereCollider.hpp"
#include "Components/Craig_CapsuleCollider.hpp"
#include "Components/Craig_RigidBody.hpp"

CraigError Craig::GameObject::init(std::string name, Craig::Scene* scenePtr) {

	CraigError ret = CRAIG_SUCCESS;

	m_name = name;
	mp_scene = scenePtr;

	mv3_position = { 0.0f, 0.0f, 0.0f };
	mv3_rotation = { 0.0f, 0.0f, 0.0f };
	m_rotationQuat = glm::quat(glm::radians(mv3_rotation));

	return ret;
}

void Craig::GameObject::setRotation(glm::vec3 rotation) {
	mv3_rotation = rotation;
	m_rotationQuat = glm::quat(glm::radians(mv3_rotation));
}

void Craig::GameObject::setRotationQuat(const glm::quat& q) {
	m_rotationQuat = glm::normalize(q);
	mv3_rotation = glm::degrees(glm::eulerAngles(m_rotationQuat));
}

CraigError Craig::GameObject::update() {

	CraigError ret = CRAIG_SUCCESS;

	for (const std::unique_ptr<Components::Component>& pComponent : mv_components)
	{
		pComponent->update();
	}

	// after the components, since some of them (RigidBody) move the object
	updateModelMatrix();

	return ret;
}


glm::mat4 Craig::GameObject::calculateModelMatrix() const
{
	return glm::translate(glm::mat4(1), mv3_position)
		* glm::mat4_cast(m_rotationQuat)
		* glm::scale(glm::mat4(1), mv3_scale);
}

void Craig::GameObject::updateModelMatrix()
{
	m_modelMatrix = calculateModelMatrix();
}


CraigError Craig::GameObject::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	for (const std::unique_ptr<Components::Component>& pComponent : mv_components)
	{
		pComponent->terminate();
	}
	mv_components.clear();

	return ret;
}

void Craig::GameObject::removeComponent(Components::Component* pComponent)
{
	assert(pComponent != nullptr);

	pComponent->terminate();

	// unique_ptr frees it once it's out of the vector
	std::erase_if(mv_components, [pComponent](const std::unique_ptr<Components::Component>& pOwned) { return pOwned.get() == pComponent; });
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
		if (Utilities::displayVectorAttribute("Rotation", mv3_rotation)) {
			m_rotationQuat = glm::quat(glm::radians(mv3_rotation));
		}
		Utilities::displayVectorAttribute("Scale", mv3_scale);
		ImGui::TreePop();
	};

	displayComponents();
}

void Craig::GameObject::displayComponents()
{
	// Can't remove mid-loop or we'd invalidate the iterator, so remember it and do it after
	Components::Component* pComponentToRemove = nullptr;

	for (const std::unique_ptr<Components::Component>& pComponent : mv_components)
	{
		ImGui::PushID(pComponent.get());
		if (ImGui::TreeNodeEx("##Component", ImGuiTreeNodeFlags_DefaultOpen, "%s", pComponent->getTypeName()))
		{
			pComponent->displayImGuiAttributes();

			if (ImGui::Button("Remove Component"))
			{
				pComponentToRemove = pComponent.get();
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	if (pComponentToRemove != nullptr)
	{
		removeComponent(pComponentToRemove);
	}

	if (ImGui::Button("Add Component"))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		// greyed out if the object already has one, colliders can go on as many times as you want
		if (ImGui::MenuItem("Model", nullptr, false, getComponent<Components::Model>() == nullptr))
		{
			addComponent<Components::Model>();
		}

		// only one sun per scene
		const bool sceneHasSun = mp_scene->getSun() != nullptr;
		if (ImGui::MenuItem("Sun", nullptr, false, !sceneHasSun))
		{
			addComponent<Components::Sun>();
		}
		if (sceneHasSun)
		{
			ImGui::SetItemTooltip("The scene already has a sun (%s)", mp_scene->getSun()->getOwner()->getName().c_str());
		}

		if (ImGui::MenuItem("Box Collider", nullptr, false))
		{
			addComponent<Components::BoxCollider>();
		}

		if (ImGui::MenuItem("Sphere Collider", nullptr, false))
		{
			addComponent<Components::SphereCollider>();
		}

		if (ImGui::MenuItem("Capsule Collider", nullptr, false))
		{
			addComponent<Components::CapsuleCollider>();
		}

		if (ImGui::MenuItem("Rigid Body", nullptr, false, getComponent<Components::RigidBody>() == nullptr))
		{
			addComponent<Components::RigidBody>();
		}

		ImGui::EndPopup();
	}
}




