#include "Craig_RigidBody.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "Craig/Craig_Logger.hpp"
#include "Craig_Collider.hpp"
#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include "Craig_GameObject.hpp"
#include "Craig_Scene.hpp"

// glm::quat is (w, x, y, z) but JPH::Quat is (x, y, z, w), so go by name
static JPH::Quat toJolt(const glm::quat& q) { return JPH::Quat(q.x, q.y, q.z, q.w); }
static JPH::Vec3 toJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }

void Craig::Components::RigidBody::createPhysicsBody()
{
	m_shapeDirty = false;

	// collider values are in the object's space so its scale has to be baked in
	// Jolt can't scale a body, only a shape
	const glm::vec3& ownerScale = mp_owner->getScale();
	mv3_builtScale = ownerScale;

	// Colliders can have no shape yet (a convex one without a model), those get skipped
	std::vector<std::pair<const Collider*, JPH::Ref<JPH::ShapeSettings>>> colliders;
	for (const Collider* pCollider : mp_owner->getComponents<Collider>())
	{
		JPH::Ref<JPH::ShapeSettings> colliderShape = pCollider->createShapeSettings(ownerScale);
		if (colliderShape != nullptr)
		{
			colliders.emplace_back(pCollider, colliderShape);
		}
	}
	if (colliders.empty())
	{
		// trace since this gets tried every update untill a collider shows up
		Craig::Logger::physics().trace("'{}' has a rigid body but no colliders with a shape yet", mp_owner->getName());
		return; // nothing to collide with yet, try again next update
	}

	// Shapes are always centred on the body, so each one gets wrapped with its collider's offset/rotation.
	// One collider only needs a RotatedTranslatedShape, more than one get put together in a compound shape
	// (which also works out the combined centre of mass). Both keep a ref to the shapes they're given.
	JPH::Ref<JPH::ShapeSettings> shapeSettings;
	if (colliders.size() == 1)
	{
		const auto& [pCollider, colliderShape] = colliders[0];
		shapeSettings = new JPH::RotatedTranslatedShapeSettings(
			toJolt(ownerScale * pCollider->getPosition()),
			toJolt(pCollider->getLocalRotation()),
			colliderShape);
	}
	else
	{
		JPH::StaticCompoundShapeSettings* pCompound = new JPH::StaticCompoundShapeSettings;
		for (const auto& [pCollider, colliderShape] : colliders)
		{
			pCompound->AddShape(
				toJolt(ownerScale * pCollider->getPosition()),
				toJolt(pCollider->getLocalRotation()),
				colliderShape);
		}
		shapeSettings = pCompound;
	}

	JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings->Create();
	if (shapeResult.HasError())
	{
		Craig::Logger::physics().error("RigidBody shape creation failed on '{}': {}", mp_owner->getName(), shapeResult.GetError().c_str());
		return;
	}

	// the body itself sits at the game object's transform
	const glm::vec3& pos = mp_owner->getPosition();
	const glm::quat& rot = mp_owner->getRotationQuat();

	// The layer filters assume anything in NON_MOVING never moves, so the motion type has to match it
	const bool isStatic = m_layer == Physics::Layers::NON_MOVING;

	JPH::BodyCreationSettings bodySettings(
		shapeResult.Get(),
		JPH::RVec3(pos.x, pos.y, pos.z),
		toJolt(rot),
		isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
		m_layer);

	m_bodyId = mp_owner->getScene()->getPhysicsEngine()->getBodyInterface()->CreateAndAddBody(bodySettings,
		isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);

	if (m_bodyId.IsInvalid())
	{
		Craig::Logger::physics().error("Jolt wouldn't make a body for '{}', probably out of bodies (kMaxBodies)", mp_owner->getName());
		return;
	}

	// trace since dragging a collider in the editor rebuilds the body every frame
	Craig::Logger::physics().trace("Made a {} body for '{}' with {} collider(s)", isStatic ? "static" : "dynamic", mp_owner->getName(), colliders.size());
}

void Craig::Components::RigidBody::destroyPhysicsBody()
{
	if (m_bodyId.IsInvalid())
	{
		return;
	}

	JPH::BodyInterface* bodyInterface = mp_owner->getScene()->getPhysicsEngine()->getBodyInterface();
	bodyInterface->RemoveBody(m_bodyId);
	bodyInterface->DestroyBody(m_bodyId);
	m_bodyId = JPH::BodyID();
}

void Craig::Components::RigidBody::pushTransformToBody()
{
	JPH::BodyInterface* bodyInterface = mp_owner->getScene()->getPhysicsEngine()->getBodyInterface();

	JPH::RVec3 bodyPos;
	JPH::Quat bodyRot;
	bodyInterface->GetPositionAndRotation(m_bodyId, bodyPos, bodyRot);

	const JPH::Vec3 objectPos = toJolt(mp_owner->getPosition());
	const JPH::Quat objectRot = toJolt(mp_owner->getRotationQuat());

	// only touch it if it moved, otherwise it'd keep waking up for nothing
	if (bodyPos.IsClose(objectPos) && bodyRot.IsClose(objectRot))
	{
		return;
	}

	// wake it up or it'll just hang there asleep wherever you dragged it
	const bool isStatic = m_layer == Physics::Layers::NON_MOVING;
	bodyInterface->SetPositionAndRotation(m_bodyId, objectPos, objectRot, isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
}

CraigError Craig::Components::RigidBody::update() {

	CraigError ret = CRAIG_SUCCESS;

	JPH::BodyInterface* bodyInterface = mp_owner->getScene()->getPhysicsEngine()->getBodyInterface();

	// Colliders changed or the object got rescaled, rebuild it but keep it moving how it was
	if (!m_bodyId.IsInvalid() && (m_shapeDirty || mp_owner->getScale() != mv3_builtScale))
	{
		const JPH::Vec3 linearVelocity = bodyInterface->GetLinearVelocity(m_bodyId);
		const JPH::Vec3 angularVelocity = bodyInterface->GetAngularVelocity(m_bodyId);

		destroyPhysicsBody();
		createPhysicsBody();

		if (!m_bodyId.IsInvalid() && m_layer != Physics::Layers::NON_MOVING)
		{
			bodyInterface->SetLinearAndAngularVelocity(m_bodyId, linearVelocity, angularVelocity);
		}
		return ret; // just made it from the object's transform, nothing to copy back yet
	}

	if (m_bodyId.IsInvalid())
	{
		createPhysicsBody();
		return ret; // just made it from the object's transform, nothing to copy back yet
	}

	// not simulating, the object's the boss
	if (!mp_owner->getScene()->getPhysicsEngine()->isSimulating())
	{
		pushTransformToBody();
		return ret;
	}

	// static bodies never move, so there's nothing to copy
	if (m_layer == Physics::Layers::NON_MOVING)
	{
		return ret;
	}

	// The body's position is the object's origin, not the centre of mass, since the collider offset lives in the shape
	JPH::RVec3 pos;
	JPH::Quat rot;
	bodyInterface->GetPositionAndRotation(m_bodyId, pos, rot);

	mp_owner->setPosition(glm::vec3(pos.GetX(), pos.GetY(), pos.GetZ()));
	// glm::quat takes (w, x, y, z)
	mp_owner->setRotationQuat(glm::quat(rot.GetW(), rot.GetX(), rot.GetY(), rot.GetZ()));

	return ret;
}


CraigError Craig::Components::RigidBody::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	destroyPhysicsBody();

	return ret;
}

CraigError Craig::Components::RigidBody::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Saved as a string so the scene file is readable, missing key keeps the default
	const std::string layer = json.value("layer", m_layer == Physics::Layers::NON_MOVING ? "nonMoving" : "moving");
	m_layer = layer == "nonMoving" ? Physics::Layers::NON_MOVING : Physics::Layers::MOVING;

	return ret;
}

void Craig::Components::RigidBody::saveToJson(nlohmann::json& json) const {

	json["layer"] = m_layer == Physics::Layers::NON_MOVING ? "nonMoving" : "moving";
}

void Craig::Components::RigidBody::displayImGuiAttributes()
{
	// indices match the layer values
	static const char* layerOptions[] = { "Non Moving (Static)", "Moving (Dynamic)" };
	int layerIndex = static_cast<int>(m_layer);
	if (ImGui::Combo("Layer", &layerIndex, layerOptions, IM_ARRAYSIZE(layerOptions)))
	{
		m_layer = static_cast<JPH::ObjectLayer>(layerIndex);
		destroyPhysicsBody(); // update() makes the new one
	}

	// Collider and scale changes get picked up on their own, this is for putting the body back
	// where the object is and stopping it
	if (ImGui::Button("Recreate Body"))
	{
		destroyPhysicsBody();
	}
}
