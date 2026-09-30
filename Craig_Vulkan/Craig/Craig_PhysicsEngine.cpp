#include "Craig_PhysicsEngine.hpp"

#include "Craig_Logger.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <thread>

// Started from Jolt's HelloWorld example

void Craig::PhysicsEngine::traceCallback(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), format, list);
	va_end(list);

	Craig::Logger::physics().info("{}", buffer);
}

bool Craig::PhysicsEngine::assertFailedCallback(const char* expression, const char* message, const char* file, JPH::uint line)
{
	Craig::Logger::physics().critical("{}:{}: ({}) {}", file, line, expression, message != nullptr ? message : "");

	// true breaks into the debugger
	return true;
}

Craig::PhysicsEngine::BroadPhaseLayerInterfaceImpl::BroadPhaseLayerInterfaceImpl()
{
	m_objectToBroadPhase[Physics::Layers::NON_MOVING] = Physics::BroadPhaseLayers::NON_MOVING;
	m_objectToBroadPhase[Physics::Layers::MOVING] = Physics::BroadPhaseLayers::MOVING;
}

JPH::uint Craig::PhysicsEngine::BroadPhaseLayerInterfaceImpl::GetNumBroadPhaseLayers() const
{
	return Physics::BroadPhaseLayers::NUM_LAYERS;
}

JPH::BroadPhaseLayer Craig::PhysicsEngine::BroadPhaseLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer layer) const
{
	JPH_ASSERT(layer < Physics::Layers::NUM_LAYERS);
	return m_objectToBroadPhase[layer];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* Craig::PhysicsEngine::BroadPhaseLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const
{
	switch (static_cast<JPH::BroadPhaseLayer::Type>(layer))
	{
	case static_cast<JPH::BroadPhaseLayer::Type>(Physics::BroadPhaseLayers::NON_MOVING):	return "NON_MOVING";
	case static_cast<JPH::BroadPhaseLayer::Type>(Physics::BroadPhaseLayers::MOVING):		return "MOVING";
	default: JPH_ASSERT(false); return "INVALID";
	}
}
#endif

bool Craig::PhysicsEngine::ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhaseLayer) const
{
	switch (layer)
	{
	case Physics::Layers::NON_MOVING:
		return broadPhaseLayer == Physics::BroadPhaseLayers::MOVING;
	case Physics::Layers::MOVING:
		return true;
	default:
		JPH_ASSERT(false);
		return false;
	}
}

bool Craig::PhysicsEngine::ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer layerA, JPH::ObjectLayer layerB) const
{
	switch (layerA)
	{
	case Physics::Layers::NON_MOVING:
		return layerB == Physics::Layers::MOVING; // Static only collides with moving
	case Physics::Layers::MOVING:
		return true; // Moving collides with everything
	default:
		JPH_ASSERT(false);
		return false;
	}
}

CraigError Craig::PhysicsEngine::init() {

	CraigError ret = CRAIG_SUCCESS;

	// Has to be before any other Jolt call. Jolt just uses malloc/free with this, see Jolt/Core/Memory.h to hook in your own
	JPH::RegisterDefaultAllocator();

	JPH::Trace = traceCallback;
	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = assertFailedCallback;)

	// Needed by RegisterTypes, it creates Jolt's classes by name (mainly for loading saved data)
	JPH::Factory::sInstance = new JPH::Factory();
	JPH::RegisterTypes();

	// scratch memory for each step, size is in Craig_Constants
	mp_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(kPhysicsTempAllocatorSize);

	// leave a thread for the main thread. hardware_concurrency can return 0 if it doesn't know
	const int workerThreads = std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
	mp_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, workerThreads);

	mp_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
	mp_physicsSystem->Init(kMaxBodies, kNumBodyMutexes, kMaxBodyPairs, kMaxContactConstraints,
		m_broadPhaseLayerInterface, m_objectVsBroadPhaseLayerFilter, m_objectLayerPairFilter);

	mp_bodyInterface = &mp_physicsSystem->GetBodyInterface();

	return ret;
}

CraigError Craig::PhysicsEngine::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

	m_timeAccumulator += deltaTime;

	// Jolt wants 1 collision step per 1/60th of a second (rounded up) to stay stable with bigger steps
	const int collisionSteps = std::max(1, static_cast<int>(std::ceil(m_fixedTimeStep * 60.0f)));

	int stepsThisFrame = 0;
	while (m_timeAccumulator >= m_fixedTimeStep && stepsThisFrame < kMaxStepsPerFrame)
	{
		mp_physicsSystem->Update(m_fixedTimeStep, collisionSteps, mp_tempAllocator.get(), mp_jobSystem.get());

		m_timeAccumulator -= m_fixedTimeStep;
		++stepsThisFrame;
	}

	// Hit the step cap and still behind, drop the leftover time instead of carrying it into the next frame
	if (m_timeAccumulator >= m_fixedTimeStep)
	{
		m_timeAccumulator = 0.0f;
	}

	return ret;
}

void Craig::PhysicsEngine::setFixedTimeStep(float timeStep)
{
	// a step of 0 would make the accumulator loop in update() never end
	m_fixedTimeStep = std::clamp(timeStep, 1.0f / 1000.0f, 1.0f / 10.0f);
}

CraigError Craig::PhysicsEngine::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	// bodies are already gone, rigid bodies remove themselves when the scenes terminate
	// everything Jolt made has to go before its types are unregistered, newest first
	mp_bodyInterface = nullptr;
	mp_physicsSystem.reset();
	mp_jobSystem.reset();
	mp_tempAllocator.reset();

	JPH::UnregisterTypes();

	delete JPH::Factory::sInstance;
	JPH::Factory::sInstance = nullptr;

	return ret;
}
