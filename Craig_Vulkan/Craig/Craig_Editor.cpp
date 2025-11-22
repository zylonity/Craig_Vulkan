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

		ImGui::DockBuilderDockWindow("###RenderingSettings", dock_id_left);
		ImGui::DockBuilderFinish(dockspace_id);

		//Default windows to open
		m_ShowRendererProperties = true;

		m_initialised = true;
	}
	


	return ret;
}


CraigError Craig::ImguiEditor::editorMain() {

	CraigError ret = CRAIG_SUCCESS;

	editorInit();

	showRenderProperties();

	return ret;
}


CraigError Craig::ImguiEditor::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

void Craig::ImguiEditor::showRenderProperties() {
	if (m_ShowRendererProperties)
	{
		// The ### is for a unique ID, otherwsise the window doesn't stay docked on the right since the name/id changes
		ImGui::Begin("Rendering Properties###RenderingSettings", &m_ShowRendererProperties);

		ImGui::SeparatorText("ImGui Info");
		ImGui::Text("Imgui Version: %s", ImGui::GetVersion());

		ImGui::SeparatorText("FPS Details");
		//ImGui::Text("Frame Time: %f", ImGui::GetIO().Framerate);
		ImGui::Text("FPS: % .2f", ImGui::GetIO().Framerate);
		ImGui::Text("Delta Time: %f", ImGui::GetIO().DeltaTime);

		ImGui::SeparatorText("Video Settings");
		if (ImGui::Checkbox("VSYNC", &mp_renderer->getVSyncState())) {
			mp_renderer->refreshSwapChain();
		}
		ImGui::SeparatorText("Camera");
		ImGui::DragFloat3("Cam Pos", glm::value_ptr(mp_camera->getPosition()));
		ImGui::DragFloat2("Cam Rot", glm::value_ptr(mp_camera->getRotation()));
		ImGui::DragFloat3("Cam Vel", glm::value_ptr(mp_camera->getVelocity()));
		//ImGui::Checkbox("Show wireframe", &mp_Renderer->getWifeFrameVisibility());*/

		ImGui::End();
	}
}



