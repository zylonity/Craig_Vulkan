#include "Craig_CapsuleCollider.hpp"
#include "Craig/Craig_GameObject.hpp"
#include "Craig/Craig_Editor.hpp"
#include "Craig/Craig_ResourceManager.hpp"
#include "Craig_Model.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

CraigError Craig::Components::CapsuleCollider::init() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Components::CapsuleCollider::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::Components::CapsuleCollider::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

void Craig::Components::CapsuleCollider::setRotation(glm::vec3 rotation) {
	mv3_capsuleRotation = rotation;
	m_capsuleRotationQuat = glm::quat(glm::radians(mv3_capsuleRotation));
}

void Craig::Components::CapsuleCollider::setRotationQuat(const glm::quat& q) {
	m_capsuleRotationQuat = glm::normalize(q);
	mv3_capsuleRotation = glm::degrees(glm::eulerAngles(m_capsuleRotationQuat));
}

void Craig::Components::CapsuleCollider::setRadius(float radius) {
	// jolt asserts on a zero/negative radius
	m_radius = glm::max(radius, 0.001f);
}

void Craig::Components::CapsuleCollider::setHalfHeight(float halfHeight) {
	// Jolt wants the cylinder bit to have some height, otherwise it's just a sphere
	m_halfHeight = glm::max(halfHeight, 0.001f);
}

glm::vec3 Craig::Components::CapsuleCollider::getWorldCentre() const {
	// Not GetModelMatrix(), that'd be a frame behind while the object's being dragged in the editor
	return glm::vec3(mp_owner->calculateModelMatrix() * glm::vec4(mv3_capsulePos, 1.0f));
}

glm::quat Craig::Components::CapsuleCollider::getWorldRotation() const {
	return mp_owner->getRotationQuat() * m_capsuleRotationQuat;
}

float Craig::Components::CapsuleCollider::getWorldRadius() const {
	const glm::vec3 ownerScale = glm::abs(mp_owner->getScale());
	return m_radius * glm::max(ownerScale.x, glm::max(ownerScale.y, ownerScale.z));
}

float Craig::Components::CapsuleCollider::getWorldHalfHeight() const {
	// How much the owner stretches things along the capsule's axis
	const glm::vec3 axis = m_capsuleRotationQuat * glm::vec3(0.0f, 1.0f, 0.0f);
	return m_halfHeight * glm::length(mp_owner->getScale() * axis);
}

bool Craig::Components::CapsuleCollider::fitToModel() {

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

	// The bounds are in the model's space, which is the same space the collider's values are in
	const glm::vec3 size = max - min;
	mv3_capsulePos = (min + max) * 0.5f;

	// Lie along the longest side, the other two sides decide how fat it is
	int longest = 0;
	for (int axis = 1; axis < 3; axis++)
	{
		if (size[axis] > size[longest])
		{
			longest = axis;
		}
	}
	const float radius = glm::max(size[(longest + 1) % 3], size[(longest + 2) % 3]) * 0.5f;
	setRadius(radius);
	// the caps take up a radius on each end
	setHalfHeight(size[longest] * 0.5f - radius);

	// The capsule stands along Y, so turn it onto the longest axis
	switch (longest)
	{
	case 0:
		setRotation(glm::vec3(0.0f, 0.0f, -90.0f)); // Y -> X
		break;
	case 2:
		setRotation(glm::vec3(90.0f, 0.0f, 0.0f)); // Y -> Z
		break;
	default:
		setRotation(glm::vec3(0.0f));
		break;
	}

	return true;
}

CraigError Craig::Components::CapsuleCollider::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Missing keys keep the defaults
	mv3_capsulePos = Utilities::readJsonVec3(json, "position", mv3_capsulePos);
	// Saved as a quat so it comes back exactly, setRotationQuat keeps the euler angles in sync
	setRotationQuat(Utilities::readJsonQuat(json, "rotation", m_capsuleRotationQuat));
	setRadius(json.value("radius", m_radius));
	setHalfHeight(json.value("halfHeight", m_halfHeight));

	return ret;
}

void Craig::Components::CapsuleCollider::saveToJson(nlohmann::json& json) const {

	Utilities::writeJsonVec3(json, "position", mv3_capsulePos);
	Utilities::writeJsonQuat(json, "rotation", m_capsuleRotationQuat);
	json["radius"] = m_radius;
	json["halfHeight"] = m_halfHeight;
}

void Craig::Components::CapsuleCollider::displayImGuiAttributes()
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

	ImGui::Checkbox("View Capsule Collider", &m_showOutline);

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
	ImGui::DragFloat3("Position", glm::value_ptr(mv3_capsulePos), 0.01f);
	// The quat is what actually gets used, so it has to be rebuilt when the euler angles change
	if (ImGui::DragFloat3("Rotation", glm::value_ptr(mv3_capsuleRotation)))
	{
		m_capsuleRotationQuat = glm::quat(glm::radians(mv3_capsuleRotation));
	}
	float radius = m_radius;
	if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.001f, FLT_MAX))
	{
		setRadius(radius);
	}
	float halfHeight = m_halfHeight;
	if (ImGui::DragFloat("Half Height", &halfHeight, 0.01f, 0.001f, FLT_MAX))
	{
		setHalfHeight(halfHeight);
	}
	ImGui::SetItemTooltip("Half the height of the straight bit in the middle, the round ends go on top of this");
}
