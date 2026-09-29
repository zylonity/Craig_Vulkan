#pragma once
#include "Craig_Collider.hpp"

namespace Craig {

	namespace Components
	{
		class SphereCollider : public Collider {

		public:

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;

			const char* getTypeName() const override { return "SphereCollider"; }
			const char* getJsonKey() const override { return "sphereCollider"; }

			float getRadius() const { return m_radius; }
			void setRadius(float radius);

			// spheres can't be squashed, so it uses the owner's biggest scale axis (same as physics)
			glm::vec3 getWorldCentre() const;
			float getWorldRadius() const;

			JPH::Ref<JPH::ShapeSettings> createShapeSettings(const glm::vec3& ownerScale) const override;

			void drawOutline(const ColliderOutlineContext& context) const override;
			glm::mat4 getGizmoMatrix() const override;
			void applyGizmoMatrix(const glm::mat4& worldMatrix, ImGuizmo::OPERATION operation) override;
			// A sphere looks the same any way round, so no rotation
			bool canRotate() const override { return false; }

		protected:
			bool displayShapeAttributes() override;
			void fitToBounds(const glm::vec3& min, const glm::vec3& max) override;

		private:
			static float biggestScale(const glm::vec3& scale);

			float m_radius = 0.5f;
		};
	}

}
