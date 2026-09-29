#pragma once
#include "Craig/Craig_Constants.hpp"
#include "Craig_Component.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Craig {

	namespace Components
	{
		class BoxCollider : public Component {

		public:

			CraigError init() override;
			CraigError update() override;
			CraigError terminate() override;

			CraigError loadFromJson(const nlohmann::json& json) override;
			void saveToJson(nlohmann::json& json) const override;
			void displayImGuiAttributes() override;

			const char* getTypeName() const override { return "BoxCollider"; }
			const char* getJsonKey() const override { return "boxCollider"; }

			const glm::vec3& getPosition() const { return mv3_boxPos; }
			const glm::vec3& getRotation() const { return mv3_boxRotation; }
			const glm::quat& getRotationQuat() const { return m_boxRotationQuat; }
			const glm::vec3& getScale() const { return mv3_boxScale; }

			void setPosition(glm::vec3 position) { mv3_boxPos = position; };
			void setRotation(glm::vec3 rotation);
			void setScale(glm::vec3 scale)		 { mv3_boxScale = scale; };
			void setRotationQuat(const glm::quat& q);

			// pos/rot/scale are relative to the owning game object
			glm::mat4 getLocalMatrix() const;
			// Owner's transform * local, where the box actually is in the world
			glm::mat4 getWorldMatrix() const;

			const bool& getSelected() const { return m_itemSelected; }
			void setSelected(bool selected) { m_itemSelected = selected; }

			// Editor only, the outline's always drawn while it's selected
			bool isOutlineVisible() const { return m_showOutline || m_itemSelected; }

			// wraps the box around the owner's model, false if there's no model
			bool fitToModel();

		private:
			// Scale is the full size of the box, so a scale of 1 is a 1x1x1 cube (corners at +-0.5)
			glm::vec3 mv3_boxPos = { 0.0f, 0.0f, 0.0f };
			glm::vec3 mv3_boxScale = { 1.0f, 1.0f, 1.0f };
			glm::vec3 mv3_boxRotation = { 0.0f, 0.0f, 0.0f };
			glm::quat m_boxRotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

			bool m_itemSelected = false;
			bool m_showOutline = false; // not saved, it's just a view setting
		};
	}

}
