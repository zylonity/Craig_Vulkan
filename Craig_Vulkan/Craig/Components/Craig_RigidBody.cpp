#include "Craig_RigidBody.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "Craig_Collider.hpp"
#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include "Craig_GameObject.hpp"
#include "Craig_Scene.hpp"

CraigError Craig::Components::RigidBody::init() {

	CraigError ret = CRAIG_SUCCESS;



	//
	// // Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
	// JPH::BoxShapeSettings shape_settings(JPH::Vec3(100.0f, 1.0f, 100.0f));
	// shape_settings.SetEmbedded(); // A ref counted object on the stack (base class RefTarget) should be marked as such to prevent it from being freed when its reference count goes to 0.
	//
	// // Create the shape
	// JPH::ShapeSettings::ShapeResult shape_result = shape_settings.Create();
	// JPH::ShapeRefC shape = shape_result.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()
	//
	// // Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
	// JPH::BodyCreationSettings floor_settings(shape, JPH::RVec3(0.0f, -1.0f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Physics::Layers::NON_MOVING);




	//
	// // Add it to the world
	// body_interface->AddBody(floor->GetID(), JPH::EActivation::DontActivate);
	return ret;
}

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
		std::cerr << "RigidBody shape creation failed: " << shapeResult.GetError() << std::endl;
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

	rb_id = mp_owner->getScene()->getPhysicsEngine()->getBodyInterface()->CreateAndAddBody(bodySettings,
		isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
}

void Craig::Components::RigidBody::destroyPhysicsBody()
{
	if (rb_id.IsInvalid())
	{
		return;
	}

	JPH::BodyInterface* bodyInterface = mp_owner->getScene()->getPhysicsEngine()->getBodyInterface();
	bodyInterface->RemoveBody(rb_id);
	bodyInterface->DestroyBody(rb_id);
	rb_id = JPH::BodyID();
}

CraigError Craig::Components::RigidBody::update() {

	CraigError ret = CRAIG_SUCCESS;

	JPH::BodyInterface* bodyInterface = mp_owner->getScene()->getPhysicsEngine()->getBodyInterface();

	// Colliders changed or the object got rescaled, rebuild it but keep it moving how it was
	if (!rb_id.IsInvalid() && (m_shapeDirty || mp_owner->getScale() != mv3_builtScale))
	{
		const JPH::Vec3 linearVelocity = bodyInterface->GetLinearVelocity(rb_id);
		const JPH::Vec3 angularVelocity = bodyInterface->GetAngularVelocity(rb_id);

		destroyPhysicsBody();
		createPhysicsBody();

		if (!rb_id.IsInvalid() && m_layer != Physics::Layers::NON_MOVING)
		{
			bodyInterface->SetLinearAndAngularVelocity(rb_id, linearVelocity, angularVelocity);
		}
		return ret; // just made it from the object's transform, nothing to copy back yet
	}

	if (rb_id.IsInvalid())
	{
		createPhysicsBody();
		return ret; // just made it from the object's transform, nothing to copy back yet
	}

	// static bodies never move, so there's nothing to copy
	if (m_layer == Physics::Layers::NON_MOVING)
	{
		return ret;
	}

	// The body's position is the object's origin, not the centre of mass, since the collider offset lives in the shape
	JPH::RVec3 pos;
	JPH::Quat rot;
	bodyInterface->GetPositionAndRotation(rb_id, pos, rot);

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
