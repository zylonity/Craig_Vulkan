#include "Craig_Editor.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_Camera.hpp"

#include "../External/Imgui/imgui.h"   
#include "../External/Imgui/imgui_internal.h"

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
	// 	// The ### is for a unique ID, otherwsise the window doesn't stay docked on the right since the name/id changes
	// 	ImGui::Begin("Scene Details###SceneDetails", &m_ShowRendererProperties);
	//
	// 	if (ImGui::CollapsingHeader("Game Objects", ImGuiTreeNodeFlags_DefaultOpen))
	// {
	// 	// Allow the user to create a new game object.
	// 	if (ImGui::Button("New Game Object"))
	// 	{
	// 		m_ShowNewGameObjectWindow = true;
	// 	}
	//
	// 	// Display all properties of game objects in the scene.
	// 	const std::vector<Craig::GameObject*>& gameOjects = mp_sceneManager->getCurrentScene()->getGameObjects();
	// 	for (Craig::GameObject* pGameObject : gameOjects)
	// 	{
	// 		// Separate the list a little for visibility
	// 		ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 4.0f);
	//
	// 		// We have to push a different ID to each node as we're using the same ID otherwise.
	// 		ImGui::PushID(pGameObject);
	// 		if (ImGui::TreeNode("##TreeNode", "%s", pGameObject->getName().c_str()))
	// 		{
	// 			// Allow the user to select the game object.
	// 			if ((pGameObject == nullptr || pGameObject != m_SelectedObject) && ImGui::Button("Select"))
	// 			{
	// 				m_SelectedObject = pGameObject;
	// 			}
	//
	// 			if (m_SelectedObject != nullptr && pGameObject == m_SelectedObject)
	// 			{
	// 				// Allow the user to deselect the game object.
	// 				if (ImGui::Button("Deselect"))
	// 				{
	// 					m_SelectedObject = nullptr;
	// 				}
	//
	// 				// Toggle translate mode on the selected game object.
	// 				if (ImGui::Button("Move"))
	// 				{
	// 					m_CurrentOperation = ImGuizmo::TRANSLATE;
	// 				}
	// 				ImGui::SameLine();
	// 				// Toggle rotate mode on the selected game object.
	// 				if (ImGui::Button("Rotate"))
	// 				{
	// 					m_CurrentOperation = ImGuizmo::ROTATE;
	// 				}
	// 				ImGui::SameLine();
	// 				// Toggle scale mode on the selected game object.
	// 				if (ImGui::Button("Scale"))
	// 				{
	// 					m_CurrentOperation = ImGuizmo::SCALE;
	// 				}
	// 			}
	//
	// 			// Allow the user to delete the game object.
	// 			if (ImGui::Button("Delete Object"))
	// 			{
	// 				// Remove the game object from the scene.
	// 				mp_SceneManager->getCurrentScene()->deleteGameObject(pGameObject);
	//
	// 				// If the deleted object is also the selected object remove it as being selected.
	// 				if (m_SelectedObject == pGameObject)
	// 				{
	// 					m_SelectedObject = nullptr;
	// 				}
	//
	// 				// We have to end imgui for this frame, otherwise we get a crash as it'll try to look through the deleted object
	// 				ImGui::TreePop();
	// 				ImGui::PopID();
	// 				break;
	// 			}
	//
	// 			// Display the objects attributes
	// 			pGameObject->displayImGuiAttributes();
	//
	// 			ImGui::TreePop();
	// 		}
	//
	// 		ImGui::PopID();
		//}
	//}

		//ImGui::End();
	}
}



