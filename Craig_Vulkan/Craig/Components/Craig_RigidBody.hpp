#pragma once
#include "Craig/Craig_Constants.hpp"
#include "Craig_Component.hpp"

#include <glm/glm.hpp>


namespace Craig {

	namespace Components
	{
		class RigidBody : public Component {

		public:

			struct Cube
			{
				glm::vec3 m_halfExtent = { 2.0f, 2.0f, 2.0f };

			};

			struct Sphere
			{


			};

			CraigError init() override;
			CraigError update() override;
			CraigError terminate() override;

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;
			void displayImGuiAttributes() override;

			const char* getTypeName() const override { return "RigidBody"; }
			const char* getJsonKey() const override { return "RigidBody"; }

			const glm::vec3& getLightDir() const { return mv3_lightDir; }
			const glm::vec3& getLightColour() const { return mv3_lightColour; }
			const glm::vec3& getAmbientColour() const { return mv3_ambientColour; }

		private:
			// defaults are the old hardcoded scene values
			glm::vec3 mv3_lightDir = { 0.5f, 1.0f, 0.25f };
			glm::vec3 mv3_lightColour = { 1.0f, 0.98f, 0.95f };
			glm::vec3 mv3_ambientColour = { 0.05f, 0.05f, 0.08f };
		};
	}

}
