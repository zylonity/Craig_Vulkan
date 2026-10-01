#pragma once
#include "Craig/Craig_Constants.hpp"

// Jolt.h has to come before any other Jolt header
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>

#include <memory>

namespace Craig {

	// Object layers decide what collides with what. Static bodies go in NON_MOVING, dynamic ones in MOVING.
	// Static bodies don't collide with each other since neither can move.
	namespace Physics::Layers
	{
		static constexpr JPH::ObjectLayer NON_MOVING = 0;
		static constexpr JPH::ObjectLayer MOVING = 1;
		static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
	};

	// Each broad phase layer is its own bounding volume tree, so the static tree doesn't have to be updated
	// when things move. One per object layer for now.
	namespace Physics::BroadPhaseLayers
	{
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
		static constexpr JPH::uint NUM_LAYERS(2);
	};

	class PhysicsEngine {

	public:

		CraigError init();
		CraigError update(const float& deltaTime);
		CraigError terminate();

		float getFixedTimeStep() const { return m_fixedTimeStep; }

		// off = update() doesn't step at all
		// the framework sets it every frame from the engine mode
		void setSimulating(bool simulating);
		// rigid bodies check this to know which way to copy
		bool isSimulating() const { return m_simulating; }
		void setFixedTimeStep(float timeStep); // clamped between 10Hz and 1000Hz

		// the locking version, it's safe to use from any thread
		JPH::BodyInterface* getBodyInterface() const { return mp_bodyInterface; }

	private:

		// Jolt's trace/assert callbacks, printed to the console
		static void traceCallback(const char* format, ...);
		static bool assertFailedCallback(const char* expression, const char* message, const char* file, JPH::uint line);

		// Maps object layers to broad phase layers
		class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
		{
		public:
			BroadPhaseLayerInterfaceImpl();
			JPH::uint GetNumBroadPhaseLayers() const override;
			JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override;
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
			const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override;
#endif
		private:
			JPH::BroadPhaseLayer m_objectToBroadPhase[Physics::Layers::NUM_LAYERS];
		};

		// Can an object layer collide with a broad phase layer (checked first, cheap)
		class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
		{
		public:
			bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const override;
		};

		// can two object layers collide
		class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
		{
		public:
			bool ShouldCollide(JPH::ObjectLayer layerA, JPH::ObjectLayer layerB) const override;
		};

		// Limits for the physics system, going over them fails body creation or drops contacts.
		// Jolt suggests 65536 bodies/body pairs and 10240 contacts for a real game.
		static constexpr JPH::uint kMaxBodies = 1024;
		static constexpr JPH::uint kNumBodyMutexes = 0; // 0 = Jolt picks
		static constexpr JPH::uint kMaxBodyPairs = 1024;
		static constexpr JPH::uint kMaxContactConstraints = 1024;

		// Caps the steps taken in one frame, so a long frame (hitch, window drag) can't snowball into more and more catch-up steps
		static constexpr int kMaxStepsPerFrame = 5;

		// The physics system keeps references to these, so they have to live as long as it does
		BroadPhaseLayerInterfaceImpl m_broadPhaseLayerInterface;
		ObjectVsBroadPhaseLayerFilterImpl m_objectVsBroadPhaseLayerFilter;
		ObjectLayerPairFilterImpl m_objectLayerPairFilter;

		std::unique_ptr<JPH::TempAllocatorImpl> mp_tempAllocator;
		std::unique_ptr<JPH::JobSystemThreadPool> mp_jobSystem;
		// Made in init(), since it has to be after Jolt's types are registered
		std::unique_ptr<JPH::PhysicsSystem> mp_physicsSystem;

		JPH::BodyInterface* mp_bodyInterface = nullptr; // Owned by the physics system

		// fixed rate, frame time goes into the accumulator and gets eaten in m_fixedTimeStep chunks
		float m_fixedTimeStep = 1.0f / 60.0f;
		float m_timeAccumulator = 0.0f;
		bool m_simulating = false; // starts off, the engine mode turns it on
		bool m_droppedTimeWarned = false; // only warn the first time, after that it's debug so it doesn't flood the log
	};

}
