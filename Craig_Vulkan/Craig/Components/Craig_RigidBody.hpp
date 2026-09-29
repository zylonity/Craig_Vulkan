#pragma once
#include "Craig/Craig_Constants.hpp"
#include "Craig_Component.hpp"
#include "Craig_PhysicsEngine.hpp"

#include <glm/glm.hpp>


namespace Craig {

	namespace Components
	{
		class RigidBody : public Component {

		public:

			CraigError init() override;
			CraigError update() override;
			CraigError terminate() override;

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;
			void displayImGuiAttributes() override;

			const char* getTypeName() const override { return "RigidBody"; }
			const char* getJsonKey() const override { return "rigidBody"; }

			// Colliders call this when they're added, removed or changed, the body gets rebuilt next update
			void markShapeDirty() { m_shapeDirty = true; }

		private:
			// Called from update() until the body exists (rb_id is valid). Done there instead of init() since init()
			// runs before loadFromJson and possibly before the colliders have been added.
			// all the object's colliders go into one body
			void createPhysicsBody();
			void destroyPhysicsBody();

			JPH::BodyID rb_id;

			bool m_shapeDirty = false;
			// The object's scale gets baked into the shapes, so the body has to be rebuilt if it changes
			glm::vec3 mv3_builtScale{};

			// NON_MOVING bodies are static, MOVING ones are dynamic
			JPH::ObjectLayer m_layer = Physics::Layers::MOVING;
		};
	}

}
