#pragma once
#include "Craig/Craig_Constants.hpp"
#include "Craig/Craig_PhysicsEngine.hpp"
#include "Craig_Component.hpp"

#include "imgui.h"
#include "ImGuizmo/ImGuizmo.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Craig {

	namespace Components
	{
		// Everything the editor needs to draw an outline, handed to each collider
		struct ColliderOutlineContext {
			ImDrawList* pDrawList = nullptr;
			glm::mat4 viewProj{};
			glm::vec2 screenSize{};
			glm::vec3 cameraPos{};
			ImU32 colour = 0;
		};

		// Base class for all the collider shapes. A game object can have as many as it wants,
		// its rigid body combines them into one body.
		class Collider : public Component {

		public:

			// tell the rigid body to rebuild, it has to include/drop this collider
			CraigError init() override;
			CraigError terminate() override;

			bool allowMultiple() const override { return true; }

			// draws the shared bits (select, gizmo buttons, fit to model, position) then the shape's own ones
			void displayImGuiAttributes() override;

			// relative to the owning game object
			const glm::vec3& getPosition() const { return mv3_position; }
			void setPosition(glm::vec3 position);
			// Identity for shapes that look the same any way round
			virtual glm::quat getLocalRotation() const { return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); }

			// the shape centred on the origin with the owner's scale baked in (Jolt shapes can't be scaled on the body)
			// The rigid body puts it at the collider's position/rotation. nullptr if it can't make one yet.
			virtual JPH::Ref<JPH::ShapeSettings> createShapeSettings(const glm::vec3& ownerScale) const = 0;

			// editor stuff
			virtual void drawOutline(const ColliderOutlineContext& context) const = 0;
			// The matrix handed to ImGuizmo, in world space
			virtual glm::mat4 getGizmoMatrix() const = 0;
			// takes what ImGuizmo did to the gizmo matrix and applies it to the collider
			virtual void applyGizmoMatrix(const glm::mat4& worldMatrix, ImGuizmo::OPERATION operation) = 0;
			virtual bool canRotate() const { return true; }

			const bool& getSelected() const { return m_itemSelected; }
			void setSelected(bool selected) { m_itemSelected = selected; }

			// Editor only, the outline's always drawn while it's selected
			bool isOutlineVisible() const { return m_showOutline || m_itemSelected; }

			// wraps the collider around the owner's model, false if there's no model
			bool fitToModel();

		protected:
			// The owner's rigid body (if it has one) rebuilds its body next update
			void markShapeDirty() const;

			// the shape's own editor bits, called after the shared ones
			// return true if anything changed so the body gets rebuilt
			virtual bool displayShapeAttributes() = 0;
			// Fit to an axis aligned box in the owner's space
			virtual void fitToBounds(const glm::vec3& min, const glm::vec3& max) = 0;

			// Saves/loads the position, for the derived classes' loadFromJson/saveToJson to call
			void loadPositionFromJson(const nlohmann::json& json);
			void savePositionToJson(nlohmann::json& json) const;

			// World space line/arc drawing for outlines, clipped where they go behind the camera
			static void drawLine(const ColliderOutlineContext& context, const glm::vec3& start, const glm::vec3& end);
			// The arc arcCentre + a * cos(t) + b * sin(t), from angle start to end
			static void drawArc(const ColliderOutlineContext& context, const glm::vec3& arcCentre, const glm::vec3& a, const glm::vec3& b, float start, float end);

			glm::vec3 mv3_position = { 0.0f, 0.0f, 0.0f };

		private:
			bool m_itemSelected = false;
			bool m_showOutline = false; // not saved, it's just a view setting
		};
	}

}
