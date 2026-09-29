#include "Craig_CapsuleCollider.hpp"
#include "Craig/Craig_GameObject.hpp"
#include "Craig/Craig_Utilities.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

void Craig::Components::CapsuleCollider::setRotation(glm::vec3 rotation) {
	mv3_capsuleRotation = rotation;
	m_capsuleRotationQuat = glm::quat(glm::radians(mv3_capsuleRotation));
	markShapeDirty();
}

void Craig::Components::CapsuleCollider::setRotationQuat(const glm::quat& q) {
	m_capsuleRotationQuat = glm::normalize(q);
	mv3_capsuleRotation = glm::degrees(glm::eulerAngles(m_capsuleRotationQuat));
	markShapeDirty();
}

void Craig::Components::CapsuleCollider::setRadius(float radius) {
	// jolt asserts on a zero/negative radius
	m_radius = glm::max(radius, 0.001f);
	markShapeDirty();
}

void Craig::Components::CapsuleCollider::setHalfHeight(float halfHeight) {
	// Jolt wants the cylinder bit to have some height, otherwise it's just a sphere
	m_halfHeight = glm::max(halfHeight, 0.001f);
	markShapeDirty();
}

float Craig::Components::CapsuleCollider::scaledRadius(const glm::vec3& ownerScale) const {
	const glm::vec3 absScale = glm::abs(ownerScale);
	return m_radius * glm::max(absScale.x, glm::max(absScale.y, absScale.z));
}

float Craig::Components::CapsuleCollider::scaledHalfHeight(const glm::vec3& ownerScale) const {
	// How much the owner stretches things along the capsule's axis
	const glm::vec3 axis = m_capsuleRotationQuat * glm::vec3(0.0f, 1.0f, 0.0f);
	return m_halfHeight * glm::length(ownerScale * axis);
}

glm::vec3 Craig::Components::CapsuleCollider::getWorldCentre() const {
	// Not GetModelMatrix(), that'd be a frame behind while the object's being dragged in the editor
	return glm::vec3(mp_owner->calculateModelMatrix() * glm::vec4(mv3_position, 1.0f));
}

glm::quat Craig::Components::CapsuleCollider::getWorldRotation() const {
	return mp_owner->getRotationQuat() * m_capsuleRotationQuat;
}

float Craig::Components::CapsuleCollider::getWorldRadius() const {
	return scaledRadius(mp_owner->getScale());
}

float Craig::Components::CapsuleCollider::getWorldHalfHeight() const {
	return scaledHalfHeight(mp_owner->getScale());
}

JPH::Ref<JPH::ShapeSettings> Craig::Components::CapsuleCollider::createShapeSettings(const glm::vec3& ownerScale) const {
	// jolt's capsule stands along Y too, so the rotation carries straight over
	return new JPH::CapsuleShapeSettings(scaledHalfHeight(ownerScale), scaledRadius(ownerScale));
}

void Craig::Components::CapsuleCollider::fitToBounds(const glm::vec3& min, const glm::vec3& max) {

	const glm::vec3 size = max - min;
	mv3_position = (min + max) * 0.5f;

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
}

void Craig::Components::CapsuleCollider::drawOutline(const ColliderOutlineContext& context) const {

	const glm::vec3 centre = getWorldCentre();
	const glm::quat rot = getWorldRotation();
	const float radius = getWorldRadius();
	const float halfHeight = getWorldHalfHeight();

	// The capsule's own axes, it stands along up
	const glm::vec3 up = rot * glm::vec3(0.0f, halfHeight, 0.0f);
	const glm::vec3 u = rot * glm::vec3(radius, 0.0f, 0.0f);
	const glm::vec3 v = rot * glm::vec3(0.0f, 0.0f, radius);
	const glm::vec3 capDir = rot * glm::vec3(0.0f, radius, 0.0f);
	const glm::vec3 top = centre + up;
	const glm::vec3 bottom = centre - up;

	// Rings where the cylinder meets the caps
	drawArc(context, top, u, v, 0.0f, glm::two_pi<float>());
	drawArc(context, bottom, u, v, 0.0f, glm::two_pi<float>());

	// Straight sides
	for (const glm::vec3& side : { u, -u, v, -v })
	{
		drawLine(context, top + side, bottom + side);
	}

	// half circles over each cap, one each way
	drawArc(context, top, u, capDir, 0.0f, glm::pi<float>());
	drawArc(context, top, v, capDir, 0.0f, glm::pi<float>());
	drawArc(context, bottom, u, -capDir, 0.0f, glm::pi<float>());
	drawArc(context, bottom, v, -capDir, 0.0f, glm::pi<float>());
}

glm::mat4 Craig::Components::CapsuleCollider::getGizmoMatrix() const {
	// Built in world space. Y is scaled to the full half length (cylinder + cap) so it's never 0, X/Z are the radius
	const float worldRadius = getWorldRadius();
	return glm::translate(glm::mat4(1.0f), getWorldCentre())
		* glm::mat4_cast(getWorldRotation())
		* glm::scale(glm::mat4(1.0f), glm::vec3(worldRadius, getWorldHalfHeight() + worldRadius, worldRadius));
}

void Craig::Components::CapsuleCollider::applyGizmoMatrix(const glm::mat4& worldMatrix, ImGuizmo::OPERATION operation) {

	const glm::vec3 scale = {
		glm::length(glm::vec3(worldMatrix[0])),
		glm::length(glm::vec3(worldMatrix[1])),
		glm::length(glm::vec3(worldMatrix[2]))
	};

	switch (operation)
	{
	case ImGuizmo::OPERATION::TRANSLATE:
	{
		// World -> the owner's space, which is what the collider's position is in
		const glm::vec3 worldCentre = glm::vec3(worldMatrix[3]);
		setPosition(glm::vec3(glm::inverse(mp_owner->calculateModelMatrix()) * glm::vec4(worldCentre, 1.0f)));
		break;
	}
	case ImGuizmo::OPERATION::ROTATE:
	{
		// Take the scale back out to get the world rotation, then the owner's rotation to get it relative to the object
		const glm::mat3 rotMat(
			glm::vec3(worldMatrix[0]) / (scale.x != 0.0f ? scale.x : 1.0f),
			glm::vec3(worldMatrix[1]) / (scale.y != 0.0f ? scale.y : 1.0f),
			glm::vec3(worldMatrix[2]) / (scale.z != 0.0f ? scale.z : 1.0f)
		);
		setRotationQuat(glm::inverse(mp_owner->getRotationQuat()) * glm::quat_cast(rotMat));
		break;
	}
	case ImGuizmo::OPERATION::SCALE:
	{
		const float worldRadius = getWorldRadius();
		const float worldHalfHeight = getWorldHalfHeight();
		const float worldHalfLength = worldHalfHeight + worldRadius;

		// X/Z both change the radius, go with whichever one changed the most
		const float ratioX = scale.x / worldRadius;
		const float ratioZ = scale.z / worldRadius;
		const float radiusRatio = glm::abs(ratioX - 1.0f) > glm::abs(ratioZ - 1.0f) ? ratioX : ratioZ;
		if (radiusRatio != 1.0f)
		{
			setRadius(m_radius * radiusRatio);
		}

		// Y stretches the whole half length, the caps stay the same so only the cylinder bit changes
		const float lengthRatio = scale.y / worldHalfLength;
		if (lengthRatio != 1.0f)
		{
			const float newWorldHalfHeight = worldHalfLength * lengthRatio - worldRadius;
			setHalfHeight(m_halfHeight * newWorldHalfHeight / worldHalfHeight);
		}
		break;
	}
	default:
		break;
	}
}

CraigError Craig::Components::CapsuleCollider::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Missing keys keep the defaults
	loadPositionFromJson(json);
	// Saved as a quat so it comes back exactly, setRotationQuat keeps the euler angles in sync
	setRotationQuat(Utilities::readJsonQuat(json, "rotation", m_capsuleRotationQuat));
	setRadius(json.value("radius", m_radius));
	setHalfHeight(json.value("halfHeight", m_halfHeight));

	return ret;
}

void Craig::Components::CapsuleCollider::saveToJson(nlohmann::json& json) const {

	savePositionToJson(json);
	Utilities::writeJsonQuat(json, "rotation", m_capsuleRotationQuat);
	json["radius"] = m_radius;
	json["halfHeight"] = m_halfHeight;
}

bool Craig::Components::CapsuleCollider::displayShapeAttributes()
{
	bool changed = false;

	// The quat is what actually gets used, so it has to be rebuilt when the euler angles change
	if (ImGui::DragFloat3("Rotation", glm::value_ptr(mv3_capsuleRotation)))
	{
		m_capsuleRotationQuat = glm::quat(glm::radians(mv3_capsuleRotation));
		changed = true;
	}
	float radius = m_radius;
	if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.001f, FLT_MAX))
	{
		setRadius(radius);
		changed = true;
	}
	float halfHeight = m_halfHeight;
	if (ImGui::DragFloat("Half Height", &halfHeight, 0.01f, 0.001f, FLT_MAX))
	{
		setHalfHeight(halfHeight);
		changed = true;
	}
	ImGui::SetItemTooltip("Half the height of the straight bit in the middle, the round ends go on top of this");

	return changed;
}
