#pragma once
#include "Craig/Craig_Constants.hpp"
#include "Craig_Component.hpp"

#include <glm/glm.hpp>

namespace Craig {

	namespace Components
	{
		class SphereCollider : public Component {

		public:

			CraigError init() override;
			CraigError update() override;
			CraigError terminate() override;

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;
			void displayImGuiAttributes() override;

			const char* getTypeName() const override { return "SphereCollider"; }
			const char* getJsonKey() const override { return "sphereCollider"; }

			const glm::vec3& getPosition() const { return mv3_spherePos; }
			float getRadius() const { return m_radius; }

			void setPosition(glm::vec3 position) { mv3_spherePos = position; };
			void setRadius(float radius);

			// spheres can't be squashed, so it uses the owner's biggest scale axis (same as physics)
			glm::vec3 getWorldCentre() const;
			float getWorldRadius() const;

			const bool& getSelected() const { return m_itemSelected; }
			void setSelected(bool selected) { m_itemSelected = selected; }

			// Editor only, the outline's always drawn while it's selected
			bool isOutlineVisible() const { return m_showOutline || m_itemSelected; }

			// wraps the sphere around the owner's model, false if there's no model
			bool fitToModel();

		private:
			// Both relative to the owning game object, no rotation since a sphere looks the same any way round
			glm::vec3 mv3_spherePos = { 0.0f, 0.0f, 0.0f };
			float m_radius = 0.5f;

			bool m_itemSelected = false;
			bool m_showOutline = false; // not saved, it's just a view setting
		};
	}

}
