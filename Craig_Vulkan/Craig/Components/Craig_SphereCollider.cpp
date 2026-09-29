#include "Craig_SphereCollider.hpp"
#include "Craig/Craig_GameObject.hpp"
#include "Craig/Craig_Editor.hpp"
#include "Craig/Craig_ResourceManager.hpp"
#include "Craig_Model.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

CraigError Craig::Components::SphereCollider::init() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Components::SphereCollider::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::Components::SphereCollider::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

void Craig::Components::SphereCollider::setRadius(float radius) {
	// jolt asserts on a zero/negative radius
	m_radius = glm::max(radius, 0.001f);
}

glm::vec3 Craig::Components::SphereCollider::getWorldCentre() const {
	// Not GetModelMatrix(), that'd be a frame behind while the object's being dragged in the editor
	return glm::vec3(mp_owner->calculateModelMatrix() * glm::vec4(mv3_spherePos, 1.0f));
}

float Craig::Components::SphereCollider::getWorldRadius() const {
	const glm::vec3 ownerScale = glm::abs(mp_owner->getScale());
	return m_radius * glm::max(ownerScale.x, glm::max(ownerScale.y, ownerScale.z));
}

bool Craig::Components::SphereCollider::fitToModel() {

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
	// Half the diagonal reaches the box's corners, so the whole model's inside
	mv3_spherePos = (min + max) * 0.5f;
	setRadius(glm::length(max - min) * 0.5f);

	return true;
}

CraigError Craig::Components::SphereCollider::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Missing keys keep the defaults
	mv3_spherePos = Utilities::readJsonVec3(json, "position", mv3_spherePos);
	setRadius(json.value("radius", m_radius));

	return ret;
}

void Craig::Components::SphereCollider::saveToJson(nlohmann::json& json) const {

	Utilities::writeJsonVec3(json, "position", mv3_spherePos);
	json["radius"] = m_radius;
}

void Craig::Components::SphereCollider::displayImGuiAttributes()
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

		// same as the game object's ones (T/E), no rotate since it wouldn't do anything
		if (ImGui::Button("Move"))
		{
			Craig::ImguiEditor::getInstance().setGizmoOperation(ImGuizmo::TRANSLATE);
		}
		ImGui::SameLine();
		if (ImGui::Button("Scale"))
		{
			Craig::ImguiEditor::getInstance().setGizmoOperation(ImGuizmo::SCALE);
		}
	}

	ImGui::Checkbox("View Sphere Collider", &m_showOutline);

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
	ImGui::DragFloat3("Position", glm::value_ptr(mv3_spherePos), 0.01f);
	float radius = m_radius;
	if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.001f, FLT_MAX))
	{
		setRadius(radius);
	}
}
