#pragma once
#include "Craig/Craig_Constants.hpp"
#include "Craig_Component.hpp"

#include <glm/glm.hpp>


namespace Craig {

	namespace Components
	{
		// Directional light for the scene, only one is allowed per scene (Scene::getSun finds it)
		class Sun : public Component {

		public:
			CraigError init() override;
			CraigError update() override;
			CraigError terminate() override;

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;
			void displayImGuiAttributes() override;

			const char* getTypeName() const override { return "Sun"; }
			const char* getJsonKey() const override { return "sun"; }

			// Points towards the sun, built from elevation + azimuth
			glm::vec3 getLightDir() const;
			const glm::vec3& getLightColour() const { return mv3_lightColour; }
			float getIntensity() const { return m_intensity; }
			const glm::vec3& getSkyColour() const { return mv3_skyColour; }
			const glm::vec3& getGroundColour() const { return mv3_groundColour; }

		private:
			// rough blackbody colour, 2000K is sunset orange, 6500K is about white
			static glm::vec3 kelvinToColour(float kelvin);

			// degrees, elevation is height above the horizon and azimuth spins around Y
			float m_elevation = 60.8f;
			float m_azimuth = 63.4f;
			glm::vec3 mv3_lightColour = { 1.0f, 0.98f, 0.95f };
			float m_intensity = 1.0f;

			// Ambient is a blend between these two based on which way a surface faces
			glm::vec3 mv3_skyColour = { 0.12f, 0.15f, 0.22f };
			glm::vec3 mv3_groundColour = { 0.06f, 0.05f, 0.04f };

			// just an editor helper for picking the colour, not saved
			float m_temperature = 5500.0f;
		};
	}

}
