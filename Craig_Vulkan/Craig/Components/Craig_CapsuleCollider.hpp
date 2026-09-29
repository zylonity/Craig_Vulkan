#pragma once
#include "Craig_Collider.hpp"

namespace Craig {

	namespace Components
	{
		// pill shape, a cylinder with a half sphere on each end
		// stands along its local Y, same as Jolt's
		class CapsuleCollider : public Collider {

		public:

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;

			const char* getTypeName() const override { return "CapsuleCollider"; }
			const char* getJsonKey() const override { return "capsuleCollider"; }

			const glm::vec3& getRotation() const { return mv3_capsuleRotation; }
			const glm::quat& getRotationQuat() const { return m_capsuleRotationQuat; }
			float getRadius() const { return m_radius; }
			float getHalfHeight() const { return m_halfHeight; }

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

			glm::quat getLocalRotation() const override { return m_capsuleRotationQuat; }
			JPH::Ref<JPH::ShapeSettings> createShapeSettings(const glm::vec3& ownerScale) const override;

			void drawOutline(const ColliderOutlineContext& context) const override;
			glm::mat4 getGizmoMatrix() const override;
			void applyGizmoMatrix(const glm::mat4& worldMatrix, ImGuizmo::OPERATION operation) override;

		protected:
			bool displayShapeAttributes() override;
			// lies along the longest side
			void fitToBounds(const glm::vec3& min, const glm::vec3& max) override;

		private:
			float scaledRadius(const glm::vec3& ownerScale) const;
			float scaledHalfHeight(const glm::vec3& ownerScale) const;

			glm::vec3 mv3_capsuleRotation = { 0.0f, 0.0f, 0.0f };
			glm::quat m_capsuleRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			float m_radius = 0.5f;
			// Half the height of just the cylinder bit, the caps go on top of this (same as Jolt)
			float m_halfHeight = 0.5f;
		};
	}

}
