#include "Craig_BoxCollider.hpp"
#include "Craig/Craig_GameObject.hpp"
#include "Craig/Craig_Utilities.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void Craig::Components::BoxCollider::setRotation(glm::vec3 rotation) {
	mv3_boxRotation = rotation;
	m_boxRotationQuat = glm::quat(glm::radians(mv3_boxRotation));
	markShapeDirty();
}

void Craig::Components::BoxCollider::setRotationQuat(const glm::quat& q) {
	m_boxRotationQuat = glm::normalize(q);
	mv3_boxRotation = glm::degrees(glm::eulerAngles(m_boxRotationQuat));
	markShapeDirty();
}

void Craig::Components::BoxCollider::setScale(glm::vec3 scale) {
	mv3_boxScale = scale;
	markShapeDirty();
}

glm::mat4 Craig::Components::BoxCollider::getLocalMatrix() const {
	return glm::translate(glm::mat4(1.0f), mv3_position)
		* glm::mat4_cast(m_boxRotationQuat)
		* glm::scale(glm::mat4(1.0f), mv3_boxScale);
}

glm::mat4 Craig::Components::BoxCollider::getWorldMatrix() const {
	// Not GetModelMatrix(), that'd be a frame behind while the object's being dragged in the editor
	return mp_owner->calculateModelMatrix() * getLocalMatrix();
}

JPH::Ref<JPH::ShapeSettings> Craig::Components::BoxCollider::createShapeSettings(const glm::vec3& ownerScale) const {

	const glm::vec3 halfExtents = glm::abs(ownerScale * mv3_boxScale) * 0.5f;

	// Jolt asserts if half extents are smaller than the convex radius, so shrink it for thin boxes
	const float convexRadius = glm::min(JPH::cDefaultConvexRadius, glm::min(halfExtents.x, glm::min(halfExtents.y, halfExtents.z)));
	return new JPH::BoxShapeSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z), convexRadius);
}

void Craig::Components::BoxCollider::fitToBounds(const glm::vec3& min, const glm::vec3& max) {
	// rotation has to be reset since the bounds are axis aligned
	mv3_position = (min + max) * 0.5f;
	mv3_boxScale = max - min; // Scale is the full size, not half
	setRotation(glm::vec3(0.0f));
}

void Craig::Components::BoxCollider::drawOutline(const ColliderOutlineContext& context) const {

	const glm::mat4 world = getWorldMatrix();

	// Corner i uses bit 0 for x, bit 1 for y, bit 2 for z (0 = -0.5, 1 = +0.5)
	glm::vec3 corners[8];
	for (int i = 0; i < 8; i++)
	{
		const glm::vec3 localCorner = {
			(i & 1) ? 0.5f : -0.5f,
			(i & 2) ? 0.5f : -0.5f,
			(i & 4) ? 0.5f : -0.5f
		};
		corners[i] = glm::vec3(world * glm::vec4(localCorner, 1.0f));
	}

	// Two corners share an edge if their indices only differ by one bit
	for (int a = 0; a < 8; a++)
	{
		for (int bit = 1; bit < 8; bit <<= 1)
		{
			const int b = a | bit;
			if (b == a)
			{
				continue; // Each edge once, from the corner with the bit unset
			}
			drawLine(context, corners[a], corners[b]);
		}
	}
}

glm::mat4 Craig::Components::BoxCollider::getGizmoMatrix() const {
	// The collider's values are relative to its game object, but the gizmo works in world space.
	// applyGizmoMatrix takes the owner back out afterwards to get local again.
	return getWorldMatrix();
}

void Craig::Components::BoxCollider::applyGizmoMatrix(const glm::mat4& worldMatrix, ImGuizmo::OPERATION operation) {

	// World -> local. If the owner has non-uniform scale and the collider is rotated this has shear in it,
	// which can't be split back into pos/rot/scale, so it'll come out slightly wrong in that case.
	const glm::mat4 local = glm::inverse(mp_owner->calculateModelMatrix()) * worldMatrix;

	// Decompose the matrix manually so rotation stays as a quaternion (no Euler jumps).
	const glm::vec3 pos = glm::vec3(local[3]);
	const glm::vec3 scale = {
		glm::length(glm::vec3(local[0])),
		glm::length(glm::vec3(local[1])),
		glm::length(glm::vec3(local[2]))
	};
	const glm::mat3 rotMat(
		glm::vec3(local[0]) / (scale.x != 0.0f ? scale.x : 1.0f),
		glm::vec3(local[1]) / (scale.y != 0.0f ? scale.y : 1.0f),
		glm::vec3(local[2]) / (scale.z != 0.0f ? scale.z : 1.0f)
	);

	switch (operation)
	{
	case ImGuizmo::OPERATION::TRANSLATE:
		setPosition(pos);
		break;
	case ImGuizmo::OPERATION::ROTATE:
		setRotationQuat(glm::quat_cast(rotMat));
		break;
	case ImGuizmo::OPERATION::SCALE:
		setScale(scale);
		break;
	default:
		break;
	}
}

CraigError Craig::Components::BoxCollider::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Missing keys keep the defaults
	loadPositionFromJson(json);
	// Saved as a quat so it comes back exactly, setRotationQuat keeps the euler angles in sync
	setRotationQuat(Utilities::readJsonQuat(json, "rotation", m_boxRotationQuat));
	setScale(Utilities::readJsonVec3(json, "scale", mv3_boxScale));

	return ret;
}

void Craig::Components::BoxCollider::saveToJson(nlohmann::json& json) const {

	savePositionToJson(json);
	Utilities::writeJsonQuat(json, "rotation", m_boxRotationQuat);
	Utilities::writeJsonVec3(json, "scale", mv3_boxScale);
}

bool Craig::Components::BoxCollider::displayShapeAttributes()
{
	bool changed = false;

	// The quat is what actually gets used, so it has to be rebuilt when the euler angles change
	if (ImGui::DragFloat3("Rotation", glm::value_ptr(mv3_boxRotation)))
	{
		m_boxRotationQuat = glm::quat(glm::radians(mv3_boxRotation));
		changed = true;
	}
	changed |= ImGui::DragFloat3("Scale", glm::value_ptr(mv3_boxScale));

	return changed;
}
