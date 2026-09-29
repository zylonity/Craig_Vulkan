#pragma once
#include "Craig_Collider.hpp"

namespace Craig {

	namespace Components
	{
		class BoxCollider : public Collider {

		public:

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;

			const char* getTypeName() const override { return "BoxCollider"; }
			const char* getJsonKey() const override { return "boxCollider"; }

			const glm::vec3& getRotation() const { return mv3_boxRotation; }
			const glm::quat& getRotationQuat() const { return m_boxRotationQuat; }
			const glm::vec3& getScale() const { return mv3_boxScale; }

			void setRotation(glm::vec3 rotation);
			void setRotationQuat(const glm::quat& q);
			void setScale(glm::vec3 scale);

			glm::quat getLocalRotation() const override { return m_boxRotationQuat; }
			JPH::Ref<JPH::ShapeSettings> createShapeSettings(const glm::vec3& ownerScale) const override;

			void drawOutline(const ColliderOutlineContext& context) const override;
			glm::mat4 getGizmoMatrix() const override;
			void applyGizmoMatrix(const glm::mat4& worldMatrix, ImGuizmo::OPERATION operation) override;

			// pos/rot/scale are relative to the owning game object
			glm::mat4 getLocalMatrix() const;
			// Owner's transform * local, where the box actually is in the world
			glm::mat4 getWorldMatrix() const;

		protected:
			bool displayShapeAttributes() override;
			void fitToBounds(const glm::vec3& min, const glm::vec3& max) override;

		private:
			// Scale is the full size of the box, so a scale of 1 is a 1x1x1 cube (corners at +-0.5)
			glm::vec3 mv3_boxScale = { 1.0f, 1.0f, 1.0f };
			glm::vec3 mv3_boxRotation = { 0.0f, 0.0f, 0.0f };
			glm::quat m_boxRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		};
	}

}
