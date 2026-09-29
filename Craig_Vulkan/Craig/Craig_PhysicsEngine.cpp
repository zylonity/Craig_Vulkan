#include "Craig_PhysicsEngine.hpp"

// STL includes
#include <iostream>
#include <cstdarg>
#include <thread>
#include <algorithm>
#include <cmath>

//This is using the hello world example from Jolt's library

// Callback for traces, connect this to your own trace function if you have one
void Craig::PhysicsEngine::TraceImpl(const char *inFMT, ...)
{
	// Format the message
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	// Print to the TTY
	std::cout << buffer << std::endl;
}

#ifdef JPH_ENABLE_ASSERTS

// Callback for asserts, connect this to your own assert handler if you have one
bool Craig::PhysicsEngine::AssertFailedImpl(const char *inExpression, const char *inMessage, const char *inFile, JPH::uint inLine)
{
	// Print to the TTY
	std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr? inMessage : "") << std::endl;

	// Breakpoint
	return true;
};

#endif // JPH_ENABLE_ASSERTS

bool Craig::PhysicsEngine::ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const
{
	switch (inObject1)
	{
	case Physics::Layers::NON_MOVING:
		return inObject2 == Physics::Layers::MOVING; // Non moving only collides with moving
	case Physics::Layers::MOVING:
		return true; // Moving collides with everything
	default:
		JPH_ASSERT(false);
		return false;
	}
}

Craig::PhysicsEngine::BPLayerInterfaceImpl::BPLayerInterfaceImpl()
{
	// Create a mapping table from object to broad phase layer
	mObjectToBroadPhase[Physics::Layers::NON_MOVING] = Physics::BroadPhaseLayers::NON_MOVING;
	mObjectToBroadPhase[Physics::Layers::MOVING] = Physics::BroadPhaseLayers::MOVING;
}

JPH::uint Craig::PhysicsEngine::BPLayerInterfaceImpl::GetNumBroadPhaseLayers() const
{
	return Physics::BroadPhaseLayers::NUM_LAYERS;
}

JPH::BroadPhaseLayer Craig::PhysicsEngine::BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const
{
	JPH_ASSERT(inLayer < Physics::Layers::NUM_LAYERS);
	return mObjectToBroadPhase[inLayer];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* Craig::PhysicsEngine::BPLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const
{
	switch (static_cast<JPH::BroadPhaseLayer::Type>(inLayer))
	{
	case static_cast<JPH::BroadPhaseLayer::Type>(Physics::BroadPhaseLayers::NON_MOVING):	return "NON_MOVING";
	case static_cast<JPH::BroadPhaseLayer::Type>(Physics::BroadPhaseLayers::MOVING):		return "MOVING";
	default: JPH_ASSERT(false); return "INVALID";
	}
}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

bool Craig::PhysicsEngine::ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const
{
	switch (inLayer1)
	{
	case Physics::Layers::NON_MOVING:
		return inLayer2 == Physics::BroadPhaseLayers::MOVING;
	case Physics::Layers::MOVING:
		return true;
	default:
		JPH_ASSERT(false);
		return false;
	}
}

JPH::ValidateResult Craig::PhysicsEngine::MyContactListener::OnContactValidate(const JPH::Body &inBody1, const JPH::Body &inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult &inCollisionResult)
{
	std::cout << "Contact validate callback" << std::endl;

	// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
	return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void Craig::PhysicsEngine::MyContactListener::OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings)
{
	std::cout << "A contact was added" << std::endl;
}

void Craig::PhysicsEngine::MyContactListener::OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings)
{
	std::cout << "A contact was persisted" << std::endl;
}

void Craig::PhysicsEngine::MyContactListener::OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair)
{
	std::cout << "A contact was removed" << std::endl;
}

void Craig::PhysicsEngine::MyBodyActivationListener::OnBodyActivated(const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData)
{
	std::cout << "A body got activated" << std::endl;
}

void Craig::PhysicsEngine::MyBodyActivationListener::OnBodyDeactivated(const JPH::BodyID &inBodyID, JPH::uint64 inBodyUserData)
{
	std::cout << "A body went to sleep" << std::endl;
}

CraigError Craig::PhysicsEngine::init() {

	CraigError ret = CRAIG_SUCCESS;

	// Register allocation hook. In this example we'll just let Jolt use malloc / free but you can override these if you want (see Memory.h).
	// This needs to be done before any other Jolt function is called.
	JPH::RegisterDefaultAllocator();

	// Install trace and assert callbacks
	JPH::Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

	// Create a factory, this class is responsible for creating instances of classes based on their name or hash and is mainly used for deserialization of saved data.
	// It is not directly used in this example but still required.
	JPH::Factory::sInstance = new JPH::Factory();

	// Register all physics types with the factory and install their collision handlers with the CollisionDispatch class.
	// If you have your own custom shape types you probably need to register their handlers with the CollisionDispatch before calling this function.
	// If you implement your own default material (PhysicsMaterial::sDefault) make sure to initialize it before this function or else this function will create one for you.
	JPH::RegisterTypes();


	temp_allocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

	job_system = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

	physics_system.Init(cMaxBodies, cNumBodyMutexes, cMaxBodyPairs, cMaxContactConstraints, broad_phase_layer_interface, object_vs_broadphase_layer_filter, object_vs_object_layer_filter);

	physics_system.SetBodyActivationListener(&body_activation_listener);

	physics_system.SetContactListener(&contact_listener);

	// The main way to interact with the bodies in the physics system is through the body interface. There is a locking and a non-locking
	// variant of this. We're going to use the locking version (even though we're not planning to access bodies from multiple threads)
	body_interface = &physics_system.GetBodyInterface();

	// Next we can create a rigid body to serve as the floor, we make a large box
	// Create the settings for the collision volume (the shape).
	// Note that for simple shapes (like boxes) you can also directly construct a BoxShape.
	JPH::BoxShapeSettings floor_shape_settings(JPH::Vec3(100.0f, 1.0f, 100.0f));
	floor_shape_settings.SetEmbedded(); // A ref counted object on the stack (base class RefTarget) should be marked as such to prevent it from being freed when its reference count goes to 0.

	// Create the shape
	JPH::ShapeSettings::ShapeResult floor_shape_result = floor_shape_settings.Create();
	JPH::ShapeRefC floor_shape = floor_shape_result.Get(); // We don't expect an error here, but you can check floor_shape_result for HasError() / GetError()

	// Create the settings for the body itself. Note that here you can also set other properties like the restitution / friction.
	JPH::BodyCreationSettings floor_settings(floor_shape, JPH::RVec3(0.0f, -1.0f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Static, Physics::Layers::NON_MOVING);

	// // Create the actual rigid body
	// floor = body_interface->CreateBody(floor_settings); // Note that if we run out of bodies this can return nullptr
	//
	// // Add it to the world
	// body_interface->AddBody(floor->GetID(), JPH::EActivation::DontActivate);
	//
	// // Now create a dynamic body to bounce on the floor
	// // Note that this uses the shorthand version of creating and adding a body to the world
	// JPH::BodyCreationSettings sphere_settings(new JPH::SphereShape(0.5f), JPH::RVec3(0.0f, 2.0f, 0.0f), JPH::Quat::sIdentity(), JPH::EMotionType::Dynamic, Physics::Layers::MOVING);
	// sphere_id = body_interface->CreateAndAddBody(sphere_settings, JPH::EActivation::Activate);
	//
	// // Now you can interact with the dynamic body, in this case we're going to give it a velocity.
	// // (note that if we had used CreateBody then we could have set the velocity straight on the body before adding it to the physics system)
	// body_interface->SetLinearVelocity(sphere_id, JPH::Vec3(0.0f, -5.0f, 0.0f));


	// Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection performance (it's pointless here because we only have 2 bodies).
	// You should definitely not call this every frame or when e.g. streaming in a new level section as it is an expensive operation.
	// Instead insert all new objects in batches instead of 1 at a time to keep the broad phase efficient.
	physics_system.OptimizeBroadPhase();

	return ret;
}

CraigError Craig::PhysicsEngine::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

	time_accumulator += deltaTime;

	// If you take larger steps than 1 / 60th of a second you need to do multiple collision steps in order to keep the simulation stable. Do 1 collision step per 1 / 60th of a second (round up).
	const int cCollisionSteps = std::max(1, static_cast<int>(std::ceil(fixed_time_step * 60.0f)));

	int steps_this_frame = 0;
	while (time_accumulator >= fixed_time_step && steps_this_frame < max_steps_per_frame)
	{
		// Next step
		++step;

		// // Output current position and velocity of the sphere
		// JPH::RVec3 position = body_interface->GetCenterOfMassPosition(sphere_id);
		// JPH::Vec3 velocity = body_interface->GetLinearVelocity(sphere_id);
		// std::cout << "Step " << step << ": Position = (" << position.GetX() << ", " << position.GetY() << ", " << position.GetZ() << "), Velocity = (" << velocity.GetX() << ", " << velocity.GetY() << ", " << velocity.GetZ() << ")" << std::endl;

		// Step the world
		physics_system.Update(fixed_time_step, cCollisionSteps, temp_allocator, job_system);

		time_accumulator -= fixed_time_step;
		++steps_this_frame;
	}

	// Hit the step cap and still behind, drop the leftover time instead of carrying it into the next frame
	if (time_accumulator >= fixed_time_step)
	{
		time_accumulator = 0.0f;
	}

	return ret;
}

void Craig::PhysicsEngine::setFixedTimeStep(float timeStep)
{
	// a step of 0 would make the accumulator loop in update() never end
	fixed_time_step = std::clamp(timeStep, 1.0f / 1000.0f, 1.0f / 10.0f);
}


CraigError Craig::PhysicsEngine::terminate() {

	CraigError ret = CRAIG_SUCCESS;


	// Remove the sphere from the physics system. Note that the sphere itself keeps all of its state and can be re-added at any time.
	body_interface->RemoveBody(sphere_id);

	// Destroy the sphere. After this the sphere ID is no longer valid.
	body_interface->DestroyBody(sphere_id);

	// Remove and destroy the floor
	body_interface->RemoveBody(floor->GetID());
	body_interface->DestroyBody(floor->GetID());

	// Unregisters all types with the factory and cleans up the default material
	JPH::UnregisterTypes();

	// Destroy the factory
	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;

	return ret;
}


