#pragma once
#include "Craig_Collider.hpp"

#include <string>
#include <utility>
#include <vector>

namespace Craig {

	namespace Components
	{
		// Shrink wraps the owner's model, the tightest convex shape around all its vertices (no dents or holes).
		// Pos/rot/scale are on top of that, so the defaults wrap the model exactly.
		class ConvexCollider : public Collider {

		public:

			// Rebuilds the hull if the owner's model changed
			CraigError update() override;

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;

			const char* getTypeName() const override { return "ConvexCollider"; }
			const char* getJsonKey() const override { return "convexCollider"; }

			const glm::vec3& getRotation() const { return mv3_convexRotation; }
			const glm::quat& getRotationQuat() const { return m_convexRotationQuat; }
			const glm::vec3& getScale() const { return mv3_convexScale; }

			void setRotation(glm::vec3 rotation);
			void setRotationQuat(const glm::quat& q);
			void setScale(glm::vec3 scale);

			glm::quat getLocalRotation() const override { return m_convexRotationQuat; }
			// nullptr if there's no model to wrap yet
			JPH::Ref<JPH::ShapeSettings> createShapeSettings(const glm::vec3& ownerScale) const override;

			void drawOutline(const ColliderOutlineContext& context) const override;
			glm::mat4 getGizmoMatrix() const override;
			void applyGizmoMatrix(const glm::mat4& worldMatrix, ImGuizmo::OPERATION operation) override;

			// pos/rot/scale are relative to the owning game object
			glm::mat4 getLocalMatrix() const;

		protected:
			bool displayShapeAttributes() override;
			// Just resets pos/rot/scale, the hull already wraps the model
			void fitToBounds(const glm::vec3& min, const glm::vec3& max) override;

		private:
			// builds the hull from the owner's model if it's changed since last time. Returns true if it did.
			bool refreshHull();

			glm::vec3 mv3_convexScale = { 1.0f, 1.0f, 1.0f };
			glm::vec3 mv3_convexRotation = { 0.0f, 0.0f, 0.0f };
			glm::quat m_convexRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

			// The hull's corners in the model's space, Jolt keeps at most 256 so rebuilding the body from these is cheap.
			// not saved, it comes from the model
			std::string m_hullModelPath;
			bool m_hullModelLoaded = false;
			std::vector<glm::vec3> mv_hullPoints;
			// Pairs of indices into mv_hullPoints, for the outline
			std::vector<std::pair<uint32_t, uint32_t>> mv_hullEdges;
		};
	}

}
