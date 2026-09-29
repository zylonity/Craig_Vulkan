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

		private:
			// Called from update() until the body exists (rb_id is valid). Done there instead of init() since init()
			// runs before loadFromJson and possibly before the collider has been added.
			void createPhysicsBody();
			void destroyPhysicsBody();

			JPH::BodyID rb_id;

			// NON_MOVING bodies are static, MOVING ones are dynamic
			JPH::ObjectLayer m_layer = Physics::Layers::MOVING;
		};
	}

}
