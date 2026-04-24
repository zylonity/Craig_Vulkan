#include "Craig_Editor.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_Camera.hpp"

#include "../External/Imgui/imgui.h"   
#include "../External/Imgui/imgui_internal.h"
#include "../External/Imgui/ImGuizmo/ImGuizmo.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Craig_SceneManager.hpp"
#include "Craig_GameObject.hpp"
#include "Craig_Scene.hpp"

CraigError Craig::ImguiEditor::editorInit() {

	CraigError ret = CRAIG_SUCCESS;

	ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

	if (m_initialised == false) {
		ImGui::DockBuilderRemoveNode(dockspace_id); // Clear out existing layout
		ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);	// Add empty node
		ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

		ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
		ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);
		ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
		ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);

		ImGui::DockBuilderDockWindow("###RenderingSettings", dock_id_right);
		ImGui::DockBuilderDockWindow("###SceneDetails", dock_id_left);
		ImGui::DockBuilderFinish(dockspace_id);

		//Default windows to open
		m_ShowRendererProperties = true;
		m_ShowSceneDetails = true;

		//When we initialise the renderer we have the max sampling level set, so for now this is good enough since we change it in both places at once
		//TODO: Keep track of the current level in the renderer, not both there and here
		m_currentMSAALevel = (int)mp_renderer->getRenderingAttachments().getMaxSamplingLevel();
		mv_MSAADropdownOptions.resize(m_MSAAIndexes[mp_renderer->getRenderingAttachments().getMaxSamplingLevel()] + 1);
		m_MSAADropdownIndex = m_MSAAIndexes[m_currentMSAALevel];

		m_initialised = true;
	}
	


	return ret;
}


CraigError Craig::ImguiEditor::editorMain(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

	editorInit();

	showRenderProperties(deltaTime);
	showSceneDetails(deltaTime);
	updateImGuizmo();

	return ret;
}


CraigError Craig::ImguiEditor::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

void Craig::ImguiEditor::showRenderProperties(const float& deltaTime) {
	if (m_ShowRendererProperties)
	{
		// The ### is for a unique ID, otherwsise the window doesn't stay docked on the right since the name/id changes
		ImGui::Begin("Rendering Properties###RenderingSettings", &m_ShowRendererProperties);

		ImGui::SeparatorText("ImGui Info");
		ImGui::Text("Imgui Version: %s", ImGui::GetVersion());

		ImGui::SeparatorText("FPS Details");
		//ImGui::Text("Frame Time: %f", ImGui::GetIO().Framerate);
		ImGui::Text("FPS: % .2f", ImGui::GetIO().Framerate);
		ImGui::Text("Delta Time: %f", deltaTime);

		ImGui::SeparatorText("Video Settings");
		if (ImGui::Checkbox("VSYNC", &mp_renderer->getVSyncState())) {
			mp_renderer->refreshSwapChain();
		}
		ImGui::SeparatorText("Camera");
		ImGui::DragFloat3("Cam Pos", glm::value_ptr(mp_camera->getPosition()));
		ImGui::DragFloat2("Cam Rot", glm::value_ptr(mp_camera->getRotation()));
		ImGui::DragFloat3("Cam Vel", glm::value_ptr(mp_camera->getVelocity()));

		ImGui::DragFloat("Camera Move Speed", &mp_camera->m_movementSpeed);
		ImGui::DragFloat("Camera Rotation Speed", &mp_camera->m_rotSpeed);
		//ImGui::Checkbox("Show wireframe", &mp_Renderer->getWifeFrameVisibility());*/

		ImGui::SeparatorText("Change the minimum texture MIP level");
		if (ImGui::SliderInt("Minimum mip level", &m_currentMipLevel, 0, kMaxLODForDebugging)) {
			mp_renderer->updateMinLOD(m_currentMipLevel);
		}

		ImGui::SeparatorText("MSAA");
		if (ImGui::Combo("MSAA level", &m_MSAADropdownIndex, mv_MSAADropdownOptions.data(), mv_MSAADropdownOptions.size())) {
			ImGui::End();
			mp_renderer->updateSamplingLevel(m_MSAAEquivalents[m_MSAADropdownIndex]);
			return;
		}

		ImGui::End();
		
	}
}

void Craig::ImguiEditor::showSceneDetails(const float& deltaTime)
{
	if (m_ShowSceneDetails)
	{
	 	// The ### is for a unique ID, otherwsise the window doesn't stay docked on the right since the name/id changes
	 	ImGui::Begin("Scene Details###SceneDetails", &m_ShowRendererProperties);

	 	if (ImGui::CollapsingHeader("Game Objects", ImGuiTreeNodeFlags_DefaultOpen))
	 	{

	 		// Display all properties of game objects in the scene.
	 		const std::vector<Craig::GameObject*>& gameOjects = mp_sceneManager->getCurrentScene()->getGameObjects();
	 		for (Craig::GameObject* pGameObject : gameOjects)
	 		{
	 			// Separate the list a little for visibility
	 			ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 4.0f);

	 			// We have to push a different ID to each node as we're using the same ID otherwise.
	 			ImGui::PushID(pGameObject);
	 			if (ImGui::TreeNode("##TreeNode", "%s", pGameObject->getName().c_str()))
	 			{
	 				// Allow the user to select the game object.
	 				if ((pGameObject == nullptr || pGameObject != mp_selectedGameObject) && ImGui::Button("Select"))
	 				{
	 					mp_selectedGameObject = pGameObject;
	 				}

	 				if (mp_selectedGameObject != nullptr && pGameObject == mp_selectedGameObject)
	 				{
	 					// Allow the user to deselect the game object.
	 					if (ImGui::Button("Deselect"))
	 					{
	 						mp_selectedGameObject = nullptr;
	 					}

	 					// Toggle translate mode on the selected game object.
	 					if (ImGui::Button("Move"))
	 					{
	 						m_CurrentOperation = ImGuizmo::TRANSLATE;
	 					}
	 					ImGui::SameLine();
	 					// Toggle rotate mode on the selected game object.
	 					if (ImGui::Button("Rotate"))
	 					{
	 						m_CurrentOperation = ImGuizmo::ROTATE;
	 					}
	 					ImGui::SameLine();
	 					// Toggle scale mode on the selected game object.
	 					if (ImGui::Button("Scale"))
	 					{
	 						m_CurrentOperation = ImGuizmo::SCALE;
	 					}
	 				}

	 				// Allow the user to delete the game object.
	 				if (ImGui::Button("Delete Object"))
	 				{
	 					// Remove the game object from the scene.
	 					mp_renderer->deleteGameObject(pGameObject);

	 					// If the deleted object is also the selected object remove it as being selected.
	 					if (mp_selectedGameObject == pGameObject)
	 					{
	 						mp_selectedGameObject = nullptr;
	 					}

	 					// We have to end imgui for this frame, otherwise we get a crash as it'll try to look through the deleted object
	 					ImGui::TreePop();
	 					ImGui::PopID();
	 					break;
	 				}

	 				// Display the objects attributes
	 				pGameObject->displayImGuiAttributes();

	 				ImGui::TreePop();
	 			}

	 			ImGui::PopID();
			}
		}

		ImGui::End();
	}
}

void Craig::ImguiEditor::updateImGuizmo()
{
	// So I was having an issue where when I scaled the object with ImGuizmo, it would set the rotation to 0,0,0 DURING the scaling, it would go back to normal after
	// But I hated that visually, I had a look online and found this issue on github
	// https://github.com/CedricGuillemet/ImGuizmo/issues/125
	// I grabbed his code and adapted it, mine was similar before

	if (mp_selectedGameObject != nullptr)
	{
		// Use hotkeys to update the current transformation.
		if (ImGui::IsKeyPressed(ImGuiKey_T))
		{
			m_CurrentOperation = ImGuizmo::TRANSLATE;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_R))
		{
			m_CurrentOperation = ImGuizmo::ROTATE;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_E))
		{
			m_CurrentOperation = ImGuizmo::SCALE;
		}

		// Set the screen rect and tell ImGuizmo how to project.
		ImGuizmo::SetOrthographic(false);
		const glm::vec2 screenSize = mp_renderer->getWindowSize();
		ImGuizmo::SetRect(0, 0, screenSize.x, screenSize.y);

		// Build the transform matrix directly from pos + quat + scale. Avoids Euler round-tripping
		// through ImGuizmo's Recompose/Decompose, which clamps Y to [-90,90] and causes jumps.
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), mp_selectedGameObject->getPosition())
			* glm::mat4_cast(mp_selectedGameObject->getRotationQuat())
			* glm::scale(glm::mat4(1.0f), mp_selectedGameObject->getScale());

		// ImGuizmo expects the opposite handedness from what we render with. Our scene renders
		// with a right-handed view/projection (GLM default) plus a Vulkan Y-flip, so we hand
		// ImGuizmo left-handed equivalents: a perspectiveLH proj and the view with its Z axis flipped.
		const Craig::Camera& camera = mp_sceneManager->getCurrentScene()->getCamera();
		const glm::mat4 projLH = glm::perspectiveLH(
			glm::radians(camera.m_fov), camera.m_aspect, camera.m_nearPlane, camera.m_farPlane);
		const glm::mat4 viewLH = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, -1.0f)) * camera.getView();

		ImGuizmo::Manipulate(
			glm::value_ptr(viewLH),
			glm::value_ptr(projLH),
			m_CurrentOperation,
			ImGuizmo::MODE::LOCAL,
			glm::value_ptr(transform)
		);

		if (ImGuizmo::IsUsing())
		{
			// Decompose the matrix manually so rotation stays as a quaternion (no Euler jumps).
			glm::vec3 pos = glm::vec3(transform[3]);
			glm::vec3 scale = {
				glm::length(glm::vec3(transform[0])),
				glm::length(glm::vec3(transform[1])),
				glm::length(glm::vec3(transform[2]))
			};
			glm::mat3 rotMat(
				glm::vec3(transform[0]) / (scale.x != 0.0f ? scale.x : 1.0f),
				glm::vec3(transform[1]) / (scale.y != 0.0f ? scale.y : 1.0f),
				glm::vec3(transform[2]) / (scale.z != 0.0f ? scale.z : 1.0f)
			);
			glm::quat rot = glm::quat_cast(rotMat);

			switch (m_CurrentOperation)
			{
			case ImGuizmo::OPERATION::TRANSLATE:
				mp_selectedGameObject->setPosition(pos);
				break;
			case ImGuizmo::OPERATION::ROTATE:
				mp_selectedGameObject->setRotationQuat(rot);
				break;
			case ImGuizmo::OPERATION::SCALE:
				mp_selectedGameObject->setScale(scale);
				break;
			default:
				break;
			}
		}
	}
}



