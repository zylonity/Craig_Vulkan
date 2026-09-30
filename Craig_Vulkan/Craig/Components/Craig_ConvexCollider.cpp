#include "Craig_ConvexCollider.hpp"
#include "Craig_Model.hpp"
#include "Craig/Craig_GameObject.hpp"
#include "Craig/Craig_ResourceManager.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "Craig/Craig_Logger.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>

#include <algorithm>
#include <set>

CraigError Craig::Components::ConvexCollider::update() {

	CraigError ret = CRAIG_SUCCESS;

	// the model can be changed/loaded after the collider's added
	if (refreshHull())
	{
		markShapeDirty();
	}

	return ret;
}

bool Craig::Components::ConvexCollider::refreshHull() {

	const Components::Model* pModelComponent = mp_owner->getComponent<Components::Model>();
	const std::string modelPath = pModelComponent != nullptr && pModelComponent->hasModel() ? pModelComponent->getModelPath() : "";

	// getModel would add an empty entry if it wasn't loaded, so check first
	Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();
	const bool modelLoaded = !modelPath.empty() && resources.isModelLoaded(modelPath);

	if (modelPath == m_hullModelPath && modelLoaded == m_hullModelLoaded)
	{
		return false; // same model as last time
	}

	m_hullModelPath = modelPath;
	m_hullModelLoaded = modelLoaded;
	mv_hullPoints.clear();
	mv_hullEdges.clear();

	if (!modelLoaded)
	{
		return true;
	}

	std::vector<glm::vec3> modelPoints;
	resources.getModel(modelPath).collectPoints(modelPoints);

	JPH::Array<JPH::Vec3> joltPoints;
	joltPoints.reserve(modelPoints.size());
	for (const glm::vec3& point : modelPoints)
	{
		joltPoints.push_back(JPH::Vec3(point.x, point.y, point.z));
	}

	// let Jolt find the hull once then just keep its corners
	JPH::ConvexHullShapeSettings hullSettings(joltPoints, 0.0f);
	hullSettings.SetEmbedded(); // On the stack, so it mustn't get freed when the refcount hits 0
	JPH::ShapeSettings::ShapeResult hullResult = hullSettings.Create();
	if (hullResult.HasError())
	{
		// happens for flat models (all points on a plane), a hull needs some depth
		Craig::Logger::physics().error("Couldn't build a convex hull for {}: {}", modelPath, hullResult.GetError().c_str());
		return true;
	}

	// Create() always makes a ConvexHullShape from these settings
	const JPH::ConvexHullShape* pHull = static_cast<const JPH::ConvexHullShape*>(hullResult.Get().GetPtr());

	// Jolt stores them relative to the centre of mass, add it back to get the model's space again
	const JPH::Vec3 centreOfMass = pHull->GetCenterOfMass();
	mv_hullPoints.reserve(pHull->GetNumPoints());
	for (JPH::uint i = 0; i < pHull->GetNumPoints(); i++)
	{
		const JPH::Vec3 point = pHull->GetPoint(i) + centreOfMass;
		mv_hullPoints.push_back(glm::vec3(point.GetX(), point.GetY(), point.GetZ()));
	}

	// Walk round each face, neighbouring faces share edges so the set gets rid of the doubles
	std::set<std::pair<uint32_t, uint32_t>> edges;
	JPH::uint faceVertices[JPH::ConvexHullShape::cMaxPointsInHull];
	for (JPH::uint face = 0; face < pHull->GetNumFaces(); face++)
	{
		const JPH::uint count = pHull->GetFaceVertices(face, JPH::ConvexHullShape::cMaxPointsInHull, faceVertices);
		for (JPH::uint i = 0; i < count; i++)
		{
			const uint32_t a = faceVertices[i];
			const uint32_t b = faceVertices[(i + 1) % count];
			edges.insert(std::minmax(a, b));
		}
	}
	mv_hullEdges.assign(edges.begin(), edges.end());

	Craig::Logger::physics().debug("Convex hull for {}: {} model points down to {} hull points, {} faces", modelPath, modelPoints.size(), mv_hullPoints.size(), pHull->GetNumFaces());

	return true;
}

void Craig::Components::ConvexCollider::setRotation(glm::vec3 rotation) {
	mv3_convexRotation = rotation;
	m_convexRotationQuat = glm::quat(glm::radians(mv3_convexRotation));
	markShapeDirty();
}

void Craig::Components::ConvexCollider::setRotationQuat(const glm::quat& q) {
	m_convexRotationQuat = glm::normalize(q);
	mv3_convexRotation = glm::degrees(glm::eulerAngles(m_convexRotationQuat));
	markShapeDirty();
}

void Craig::Components::ConvexCollider::setScale(glm::vec3 scale) {
	mv3_convexScale = scale;
	markShapeDirty();
}

glm::mat4 Craig::Components::ConvexCollider::getLocalMatrix() const {
	return glm::translate(glm::mat4(1.0f), mv3_position)
		* glm::mat4_cast(m_convexRotationQuat)
		* glm::scale(glm::mat4(1.0f), mv3_convexScale);
}

JPH::Ref<JPH::ShapeSettings> Craig::Components::ConvexCollider::createShapeSettings(const glm::vec3& ownerScale) const {

	if (mv_hullPoints.empty())
	{
		return nullptr;
	}

	// The rigid body puts the shape at ownerScale * position, rotated by the collider's rotation.
	// We want each point to end up at ownerScale * (position + rotation * (scale * point)), so the rotation gets taken
	// back out after the owner's scale goes on. Since it's just points this is exact, even with non-uniform scale.
	const glm::quat inverseRotation = glm::inverse(m_convexRotationQuat);

	JPH::Array<JPH::Vec3> points;
	points.reserve(mv_hullPoints.size());
	for (const glm::vec3& hullPoint : mv_hullPoints)
	{
		const glm::vec3 point = inverseRotation * (ownerScale * (m_convexRotationQuat * (mv3_convexScale * hullPoint)));
		points.push_back(JPH::Vec3(point.x, point.y, point.z));
	}

	return new JPH::ConvexHullShapeSettings(points);
}

void Craig::Components::ConvexCollider::fitToBounds(const glm::vec3& min, const glm::vec3& max) {
	mv3_position = glm::vec3(0.0f);
	mv3_convexScale = glm::vec3(1.0f);
	setRotation(glm::vec3(0.0f));
}

void Craig::Components::ConvexCollider::drawOutline(const ColliderOutlineContext& context) const {

	// Not GetModelMatrix(), that'd be a frame behind while the object's being dragged in the editor
	const glm::mat4 world = mp_owner->calculateModelMatrix() * getLocalMatrix();

	std::vector<glm::vec3> worldPoints;
	worldPoints.reserve(mv_hullPoints.size());
	for (const glm::vec3& point : mv_hullPoints)
	{
		worldPoints.push_back(glm::vec3(world * glm::vec4(point, 1.0f)));
	}

	for (const std::pair<uint32_t, uint32_t>& edge : mv_hullEdges)
	{
		drawLine(context, worldPoints[edge.first], worldPoints[edge.second]);
	}
}

glm::mat4 Craig::Components::ConvexCollider::getGizmoMatrix() const {
	// The collider's values are relative to its game object, but the gizmo works in world space.
	// applyGizmoMatrix takes the owner back out afterwards to get local again.
	return mp_owner->calculateModelMatrix() * getLocalMatrix();
}

void Craig::Components::ConvexCollider::applyGizmoMatrix(const glm::mat4& worldMatrix, ImGuizmo::OPERATION operation) {

	// world -> local, same as the box collider's
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

CraigError Craig::Components::ConvexCollider::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Missing keys keep the defaults
	loadPositionFromJson(json);
	// Saved as a quat so it comes back exactly, setRotationQuat keeps the euler angles in sync
	setRotationQuat(Utilities::readJsonQuat(json, "rotation", m_convexRotationQuat));
	setScale(Utilities::readJsonVec3(json, "scale", mv3_convexScale));

	// The model component loads first, so the hull can be built straight away
	refreshHull();

	return ret;
}

void Craig::Components::ConvexCollider::saveToJson(nlohmann::json& json) const {

	savePositionToJson(json);
	Utilities::writeJsonQuat(json, "rotation", m_convexRotationQuat);
	Utilities::writeJsonVec3(json, "scale", mv3_convexScale);
}

bool Craig::Components::ConvexCollider::displayShapeAttributes()
{
	bool changed = false;

	// The quat is what actually gets used, so it has to be rebuilt when the euler angles change
	if (ImGui::DragFloat3("Rotation", glm::value_ptr(mv3_convexRotation)))
	{
		m_convexRotationQuat = glm::quat(glm::radians(mv3_convexRotation));
		changed = true;
	}
	changed |= ImGui::DragFloat3("Scale", glm::value_ptr(mv3_convexScale), 0.01f);

	if (mv_hullPoints.empty())
	{
		ImGui::TextColored({ 1.0f, 0.5f, 0.0f, 1.0f }, "No hull, the object needs a model component with a model loaded");
	}
	else
	{
		ImGui::Text("Hull has %zu points", mv_hullPoints.size());
	}

	return changed;
}
