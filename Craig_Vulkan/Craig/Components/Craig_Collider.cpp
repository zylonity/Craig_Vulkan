#include "Craig_Collider.hpp"
#include "Craig_Model.hpp"
#include "Craig_RigidBody.hpp"
#include "Craig/Craig_GameObject.hpp"
#include "Craig/Craig_Editor.hpp"
#include "Craig/Craig_ResourceManager.hpp"
#include "Craig/Craig_Utilities.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

CraigError Craig::Components::Collider::init() {

	markShapeDirty();
	return CRAIG_SUCCESS;
}

CraigError Craig::Components::Collider::terminate() {

	markShapeDirty();
	return CRAIG_SUCCESS;
}

void Craig::Components::Collider::setPosition(glm::vec3 position) {
	mv3_position = position;
	markShapeDirty();
}

void Craig::Components::Collider::markShapeDirty() const {
	if (mp_owner == nullptr)
	{
		return;
	}

	if (RigidBody* pRigidBody = mp_owner->getComponent<RigidBody>())
	{
		pRigidBody->markShapeDirty();
	}
}

bool Craig::Components::Collider::fitToModel() {

	const Components::Model* pModelComponent = mp_owner->getComponent<Components::Model>();
	if (pModelComponent == nullptr || !pModelComponent->hasModel())
	{
		return false;
	}

	// getModel would add an empty entry if it wasn't loaded, so check first
	Craig::ResourceManager& resources = Craig::ResourceManager::getInstance();
	if (!resources.isModelLoaded(pModelComponent->getModelPath()))
	{
		return false;
	}

	glm::vec3 min, max;
	if (!resources.getModel(pModelComponent->getModelPath()).getBounds(min, max))
	{
		return false;
	}

	// The bounds are in the model's space, which is the same space the collider's values are in
	fitToBounds(min, max);
	markShapeDirty();

	return true;
}

void Craig::Components::Collider::loadPositionFromJson(const nlohmann::json& json) {
	// missing key keeps the default
	mv3_position = Utilities::readJsonVec3(json, "position", mv3_position);
}

void Craig::Components::Collider::savePositionToJson(nlohmann::json& json) const {
	Utilities::writeJsonVec3(json, "position", mv3_position);
}

void Craig::Components::Collider::displayImGuiAttributes()
{
	// Allow the user to select the collider.
	// Goes through the editor so whatever else was selected (object or collider) gets deselected
	if (m_itemSelected == false && ImGui::Button("Select"))
	{
		Craig::ImguiEditor::getInstance().selectCollider(this);
	}
	if (m_itemSelected == true)
	{
		if (ImGui::Button("Deselect"))
		{
			m_itemSelected = false;
		}

		// Same as the game object's ones, T/R/E hotkeys work too
		if (ImGui::Button("Move"))
		{
			Craig::ImguiEditor::getInstance().setGizmoOperation(ImGuizmo::TRANSLATE);
		}
		if (canRotate())
		{
			ImGui::SameLine();
			if (ImGui::Button("Rotate"))
			{
				Craig::ImguiEditor::getInstance().setGizmoOperation(ImGuizmo::ROTATE);
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Scale"))
		{
			Craig::ImguiEditor::getInstance().setGizmoOperation(ImGuizmo::SCALE);
		}
	}

	ImGui::Checkbox("View Collider", &m_showOutline);

	// greyed out when there's nothing to fit to
	const Components::Model* pModelComponent = mp_owner->getComponent<Components::Model>();
	const bool canFit = pModelComponent != nullptr && pModelComponent->hasModel();
	ImGui::BeginDisabled(!canFit);
	if (ImGui::Button("Fit To Model"))
	{
		fitToModel();
	}
	ImGui::EndDisabled();
	if (!canFit)
	{
		ImGui::SetItemTooltip("The object needs a model component with a model loaded");
	}

	bool changed = ImGui::DragFloat3("Position", glm::value_ptr(mv3_position), 0.01f);
	changed |= displayShapeAttributes();
	if (changed)
	{
		markShapeDirty();
	}
}

// Projects a clip space point to window pixels. Uses the same un-flipped proj as ImGuizmo, so NDC Y points up
// and has to be flipped since ImGui's screen Y goes down.
static ImVec2 clipToScreen(const glm::vec4& clipPos, const glm::vec2& screenSize)
{
	const glm::vec2 ndc = glm::vec2(clipPos) / clipPos.w;
	return ImVec2(
		(ndc.x * 0.5f + 0.5f) * screenSize.x,
		(1.0f - (ndc.y * 0.5f + 0.5f)) * screenSize.y
	);
}

void Craig::Components::Collider::drawLine(const ColliderOutlineContext& context, const glm::vec3& start, const glm::vec3& end)
{
	glm::vec4 clipStart = context.viewProj * glm::vec4(start, 1.0f);
	glm::vec4 clipEnd = context.viewProj * glm::vec4(end, 1.0f);

	// Anything with w below this is (nearly) behind the camera, dividing by it would flip it across the screen
	constexpr float kMinW = 0.0001f;

	if (clipStart.w < kMinW && clipEnd.w < kMinW)
	{
		return;
	}
	if (clipStart.w < kMinW)
	{
		clipStart = glm::mix(clipStart, clipEnd, (kMinW - clipStart.w) / (clipEnd.w - clipStart.w));
	}
	else if (clipEnd.w < kMinW)
	{
		clipEnd = glm::mix(clipEnd, clipStart, (kMinW - clipEnd.w) / (clipStart.w - clipEnd.w));
	}

	context.pDrawList->AddLine(clipToScreen(clipStart, context.screenSize), clipToScreen(clipEnd, context.screenSize), context.colour, 2.0f);
}

void Craig::Components::Collider::drawArc(const ColliderOutlineContext& context, const glm::vec3& arcCentre, const glm::vec3& a, const glm::vec3& b, float start, float end)
{
	constexpr int kSegments = 48; // for a full circle, smaller arcs get fewer

	const int segments = glm::max(1, static_cast<int>(kSegments * glm::abs(end - start) / glm::two_pi<float>()));
	glm::vec3 previous = arcCentre + a * glm::cos(start) + b * glm::sin(start);
	for (int i = 1; i <= segments; i++)
	{
		const float angle = start + (end - start) * static_cast<float>(i) / segments;
		const glm::vec3 current = arcCentre + a * glm::cos(angle) + b * glm::sin(angle);
		drawLine(context, previous, current);
		previous = current;
	}
}
