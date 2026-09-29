#include "Craig_BoxCollider.hpp"
#include "Craig/Craig_GameObject.hpp"
#include "Craig/Craig_Editor.hpp"
#include "Craig/Craig_ResourceManager.hpp"
#include "Craig_Model.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

CraigError Craig::Components::BoxCollider::init() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Components::BoxCollider::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::Components::BoxCollider::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

void Craig::Components::BoxCollider::setRotation(glm::vec3 rotation) {
	mv3_boxRotation = rotation;
	m_boxRotationQuat = glm::quat(glm::radians(mv3_boxRotation));
}

void Craig::Components::BoxCollider::setRotationQuat(const glm::quat& q) {
	m_boxRotationQuat = glm::normalize(q);
	mv3_boxRotation = glm::degrees(glm::eulerAngles(m_boxRotationQuat));
}

glm::mat4 Craig::Components::BoxCollider::getLocalMatrix() const {
	return glm::translate(glm::mat4(1.0f), mv3_boxPos)
		* glm::mat4_cast(m_boxRotationQuat)
		* glm::scale(glm::mat4(1.0f), mv3_boxScale);
}

glm::mat4 Craig::Components::BoxCollider::getWorldMatrix() const {
	// Not GetModelMatrix(), that'd be a frame behind while the object's being dragged in the editor
	return mp_owner->calculateModelMatrix() * getLocalMatrix();
}

bool Craig::Components::BoxCollider::fitToModel() {

	const Components::Model* pModelComponent = mp_owner->getComponent<Components::Model>();
	if (pModelComponent == nullptr || !pModelComponent->hasModel())
	{
		return false;
	}

	// getModel would add an empty entry if it wasn't loaded, so check first
	Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();
	if (!resources.isModelLoaded(pModelComponent->getModelPath()))
	{
		return false;
	}

	glm::vec3 min, max;
	if (!resources.getModel(pModelComponent->getModelPath()).calculateBounds(min, max))
	{
		return false;
	}

	// the bounds are in the model's space, same as the collider's values
	// rotation has to be reset since the bounds are axis aligned
	mv3_boxPos = (min + max) * 0.5f;
	mv3_boxScale = max - min; // Scale is the full size, not half
	setRotation(glm::vec3(0.0f));

	return true;
}

CraigError Craig::Components::BoxCollider::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Missing keys keep the defaults
	mv3_boxPos = Utilities::readJsonVec3(json, "position", mv3_boxPos);
	// Saved as a quat so it comes back exactly, setRotationQuat keeps the euler angles in sync
	setRotationQuat(Utilities::readJsonQuat(json, "rotation", m_boxRotationQuat));
	mv3_boxScale = Utilities::readJsonVec3(json, "scale", mv3_boxScale);

	return ret;
}

void Craig::Components::BoxCollider::saveToJson(nlohmann::json& json) const {

	Utilities::writeJsonVec3(json, "position", mv3_boxPos);
	Utilities::writeJsonQuat(json, "rotation", m_boxRotationQuat);
	Utilities::writeJsonVec3(json, "scale", mv3_boxScale);
}

void Craig::Components::BoxCollider::displayImGuiAttributes()
{// Allow the user to select the game object.
	if (m_itemSelected == false && ImGui::Button("Select"))
	{
		m_itemSelected = true;
	}
	if (m_itemSelected == true)
	{
		if (ImGui::Button("Deselect"))
		{
			m_itemSelected = false;
		}

		// Same as the game object's ones, T/R/E hotkeys work too
		if (ImGui::Button("Move"))
		{
			Craig::ImguiEditor::getInstance().setGizmoOperation(ImGuizmo::TRANSLATE);
		}
		ImGui::SameLine();
		if (ImGui::Button("Rotate"))
		{
			Craig::ImguiEditor::getInstance().setGizmoOperation(ImGuizmo::ROTATE);
		}
		ImGui::SameLine();
		if (ImGui::Button("Scale"))
		{
			Craig::ImguiEditor::getInstance().setGizmoOperation(ImGuizmo::SCALE);
		}
	}

	ImGui::Checkbox("View Box Collider", &m_showOutline);

	// greyed out when there's nothing to fit to
	const Components::Model* pModelComponent = mp_owner->getComponent<Components::Model>();
	const bool canFit = pModelComponent != nullptr && pModelComponent->hasModel();
	ImGui::BeginDisabled(!canFit);
	if (ImGui::Button("Fit To Model"))
	{
		fitToModel();
	}
	ImGui::EndDisabled();
	if (!canFit)
	{
		ImGui::SetItemTooltip("The object needs a model component with a model loaded");
	}
	ImGui::DragFloat3("Position", glm::value_ptr(mv3_boxPos), 0.01f);
	// The quat is what actually gets used, so it has to be rebuilt when the euler angles change
	if (ImGui::DragFloat3("Rotation", glm::value_ptr(mv3_boxRotation)))
	{
		m_boxRotationQuat = glm::quat(glm::radians(mv3_boxRotation));
	}
	ImGui::DragFloat3("Scale", glm::value_ptr(mv3_boxScale));
}
