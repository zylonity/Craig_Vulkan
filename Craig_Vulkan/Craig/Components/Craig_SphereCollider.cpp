#include "Craig_SphereCollider.hpp"
#include "Craig/Craig_GameObject.hpp"
#include "Craig/Craig_Utilities.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

float Craig::Components::SphereCollider::biggestScale(const glm::vec3& scale) {
	const glm::vec3 absScale = glm::abs(scale);
	return glm::max(absScale.x, glm::max(absScale.y, absScale.z));
}

void Craig::Components::SphereCollider::setRadius(float radius) {
	// jolt asserts on a zero/negative radius
	m_radius = glm::max(radius, 0.001f);
	markShapeDirty();
}

glm::vec3 Craig::Components::SphereCollider::getWorldCentre() const {
	// Not GetModelMatrix(), that'd be a frame behind while the object's being dragged in the editor
	return glm::vec3(mp_owner->calculateModelMatrix() * glm::vec4(mv3_position, 1.0f));
}

float Craig::Components::SphereCollider::getWorldRadius() const {
	return m_radius * biggestScale(mp_owner->getScale());
}

JPH::Ref<JPH::ShapeSettings> Craig::Components::SphereCollider::createShapeSettings(const glm::vec3& ownerScale) const {
	return new JPH::SphereShapeSettings(m_radius * biggestScale(ownerScale));
}

void Craig::Components::SphereCollider::fitToBounds(const glm::vec3& min, const glm::vec3& max) {
	// Half the diagonal reaches the box's corners, so the whole model's inside
	mv3_position = (min + max) * 0.5f;
	setRadius(glm::length(max - min) * 0.5f);
}

void Craig::Components::SphereCollider::drawOutline(const ColliderOutlineContext& context) const {

	const glm::vec3 centre = getWorldCentre();
	const float radius = getWorldRadius();

	// a circle around each world axis, that's the shape physics actually uses
	for (int axis = 0; axis < 3; axis++)
	{
		// the two axes the circle lies in
		glm::vec3 u(0.0f), v(0.0f);
		u[(axis + 1) % 3] = radius;
		v[(axis + 2) % 3] = radius;
		drawArc(context, centre, u, v, 0.0f, glm::two_pi<float>());
	}

	// the axis circles squash into ellipses from most angles, so draw the sphere's actual edge too
	// With perspective that edge isn't a great circle, it's a smaller one pulled towards the camera
	// (where the lines from the camera just touch the sphere). Nothing to draw if the camera's inside it.
	const glm::vec3 toCamera = context.cameraPos - centre;
	const float distance = glm::length(toCamera);
	if (distance > radius)
	{
		const glm::vec3 n = toCamera / distance;
		const glm::vec3 edgeCentre = centre + n * (radius * radius / distance);
		const float edgeRadius = radius * glm::sqrt(1.0f - (radius * radius) / (distance * distance));

		// Any two directions at right angles to n, the reference just can't be parallel to it
		const glm::vec3 reference = glm::abs(n.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		const glm::vec3 u = glm::normalize(glm::cross(n, reference));
		const glm::vec3 v = glm::cross(n, u);
		drawArc(context, edgeCentre, u * edgeRadius, v * edgeRadius, 0.0f, glm::two_pi<float>());
	}
}

glm::mat4 Craig::Components::SphereCollider::getGizmoMatrix() const {
	// Built in world space from the centre and radius, with the owner's rotation so the handles line up with the object
	return glm::translate(glm::mat4(1.0f), getWorldCentre())
		* glm::mat4_cast(mp_owner->getRotationQuat())
		* glm::scale(glm::mat4(1.0f), glm::vec3(getWorldRadius()));
}

void Craig::Components::SphereCollider::applyGizmoMatrix(const glm::mat4& worldMatrix, ImGuizmo::OPERATION operation) {

	if (operation == ImGuizmo::TRANSLATE)
	{
		// World -> the owner's space, which is what the collider's position is in
		const glm::vec3 worldCentre = glm::vec3(worldMatrix[3]);
		setPosition(glm::vec3(glm::inverse(mp_owner->calculateModelMatrix()) * glm::vec4(worldCentre, 1.0f)));
	}
	else if (operation == ImGuizmo::SCALE)
	{
		// Dragging one axis only scales that axis, so go with whichever one changed the most
		const float worldRadius = getWorldRadius();
		float bestRatio = 1.0f;
		for (int axis = 0; axis < 3; axis++)
		{
			const float ratio = glm::length(glm::vec3(worldMatrix[axis])) / worldRadius;
			if (glm::abs(ratio - 1.0f) > glm::abs(bestRatio - 1.0f))
			{
				bestRatio = ratio;
			}
		}
		setRadius(m_radius * bestRatio);
	}
}

CraigError Craig::Components::SphereCollider::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Missing keys keep the defaults
	loadPositionFromJson(json);
	setRadius(json.value("radius", m_radius));

	return ret;
}

void Craig::Components::SphereCollider::saveToJson(nlohmann::json& json) const {

	savePositionToJson(json);
	json["radius"] = m_radius;
}

bool Craig::Components::SphereCollider::displayShapeAttributes()
{
	float radius = m_radius;
	if (ImGui::DragFloat("Radius", &radius, 0.01f, 0.001f, FLT_MAX))
	{
		setRadius(radius);
		return true;
	}
	return false;
}
