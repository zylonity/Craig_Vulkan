#pragma once
#include "Craig/Craig_Constants.hpp"
#include "Craig_Component.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Craig {

	namespace Components
	{
		// pill shape, a cylinder with a half sphere on each end
		// stands along its local Y, same as Jolt's
		class CapsuleCollider : public Component {

		public:

			CraigError init() override;
			CraigError update() override;
			CraigError terminate() override;

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;
			void displayImGuiAttributes() override;

			const char* getTypeName() const override { return "CapsuleCollider"; }
			const char* getJsonKey() const override { return "capsuleCollider"; }

			const glm::vec3& getPosition() const { return mv3_capsulePos; }
			const glm::vec3& getRotation() const { return mv3_capsuleRotation; }
			const glm::quat& getRotationQuat() const { return m_capsuleRotationQuat; }
			float getRadius() const { return m_radius; }
			float getHalfHeight() const { return m_halfHeight; }

			void setPosition(glm::vec3 position) { mv3_capsulePos = position; };
			void setRotation(glm::vec3 rotation);
			void setRotationQuat(const glm::quat& q);
			void setRadius(float radius);
			void setHalfHeight(float halfHeight);

			// where the capsule actually is in the world, owner's transform applied
			// Capsules can't be squashed, so the radius uses the owner's biggest scale axis and the height uses
			// the owner's scale along the capsule's axis. Physics uses the same thing.
			glm::vec3 getWorldCentre() const;
			glm::quat getWorldRotation() const;
			float getWorldRadius() const;
			float getWorldHalfHeight() const;

			const bool& getSelected() const { return m_itemSelected; }
			void setSelected(bool selected) { m_itemSelected = selected; }

			// Editor only, the outline's always drawn while it's selected
			bool isOutlineVisible() const { return m_showOutline || m_itemSelected; }

			// resizes the capsule to wrap the owner's model, lying along its longest side
			// false if there's no model to fit to
			bool fitToModel();

		private:
			// all relative to the owning game object
			glm::vec3 mv3_capsulePos = { 0.0f, 0.0f, 0.0f };
			glm::vec3 mv3_capsuleRotation = { 0.0f, 0.0f, 0.0f };
			glm::quat m_capsuleRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			float m_radius = 0.5f;
			// Half the height of just the cylinder bit, the caps go on top of this (same as Jolt)
			float m_halfHeight = 0.5f;

			bool m_itemSelected = false;
			bool m_showOutline = false; // not saved, it's just a view setting
		};
	}

}
