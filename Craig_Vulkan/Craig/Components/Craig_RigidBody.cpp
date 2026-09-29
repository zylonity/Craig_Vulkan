#include "Craig_RigidBody.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "Craig_BoxCollider.hpp"
#include "Craig_SphereCollider.hpp"
#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

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

void Craig::Components::RigidBody::createPhysicsBody()
{
	// box wins if the object has both
	const Craig::Components::BoxCollider* boxCo = mp_owner->getComponent<BoxCollider>();
	const Craig::Components::SphereCollider* sphereCo = boxCo == nullptr ? mp_owner->getComponent<SphereCollider>() : nullptr;
	if (boxCo == nullptr && sphereCo == nullptr)
	{
		return; // nothing to collide with yet, try again next update
	}

	// collider values are in the object's space so its scale has to be baked in
	// Jolt can't scale a body, only a shape
	const glm::vec3& ownerScale = mp_owner->getScale();
	const glm::vec3 offset = ownerScale * (boxCo != nullptr ? boxCo->getPosition() : sphereCo->getPosition());
	// spheres look the same any way round so they don't have a rotation
	const glm::quat colliderRot = boxCo != nullptr ? boxCo->getRotationQuat() : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

	// On the heap (ref counted) so it outlives this if/else, the RotatedTranslatedShapeSettings keeps a ref to it
	JPH::Ref<JPH::ShapeSettings> innerSettings;
	if (boxCo != nullptr)
	{
		const glm::vec3 halfExtents = glm::abs(ownerScale * boxCo->getScale()) * 0.5f;

		// Jolt asserts if half extents are smaller than the convex radius, so shrink it for thin boxes
		const float convexRadius = glm::min(JPH::cDefaultConvexRadius, glm::min(halfExtents.x, glm::min(halfExtents.y, halfExtents.z)));
		innerSettings = new JPH::BoxShapeSettings(JPH::Vec3(halfExtents.x, halfExtents.y, halfExtents.z), convexRadius);
	}
	else
	{
		// Already has the owner's scale in it (biggest axis, since spheres can't be squashed)
		innerSettings = new JPH::SphereShapeSettings(sphereCo->getWorldRadius());
	}

	// shapes are always centred on the body, this wraps it so the collider's offset/rotation still work
	// glm::quat is (w, x, y, z) but JPH::Quat is (x, y, z, w), so go by name
	JPH::RotatedTranslatedShapeSettings shapeSettings(
		JPH::Vec3(offset.x, offset.y, offset.z),
		JPH::Quat(colliderRot.x, colliderRot.y, colliderRot.z, colliderRot.w),
		innerSettings);
	shapeSettings.SetEmbedded();

	JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
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
		JPH::Quat(rot.x, rot.y, rot.z, rot.w),
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
	mp_owner->getScene()->getPhysicsEngine()->getBodyInterface()->GetPositionAndRotation(rb_id, pos, rot);

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

	// picks up collider/transform changes, and puts the body back where the object is
	if (ImGui::Button("Recreate Body"))
	{
		destroyPhysicsBody();
	}
}
