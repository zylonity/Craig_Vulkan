#include "Craig_Editor.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_Camera.hpp"
#include "Craig_PhysicsEngine.hpp"

#include "../External/Imgui/imgui.h"
#include "../External/Imgui/imfilebrowser.h"
#include "../External/Imgui/imgui_stdlib.h"
#include "../External/Imgui/imgui_internal.h"
#include "../External/Imgui/ImGuizmo/ImGuizmo.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>

#include "Craig_SceneManager.hpp"
#include "Craig_GameObject.hpp"
#include "Craig_Scene.hpp"
#include "Craig_Utilities.hpp"
#include "../External/json.hpp"

#include <filesystem>
#include <fstream>

#include "Components/Craig_BoxCollider.hpp"

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

	// menu bar goes first so the dockspace fits underneath it
	showMainMenuBar();
	editorInit();

	showRenderProperties(deltaTime);
	showSceneDetails(deltaTime);
	updateImGuizmo();
	updateImGuizmoBoxCollider();
	updateImGuizmoSphereCollider();
	drawBoxColliderOutlines();
	drawSphereColliderOutlines();

	renderNewGameObjectWindow();
	renderNewSceneWindow();

	return ret;
}


CraigError Craig::ImguiEditor::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

void Craig::ImguiEditor::showMainMenuBar()
{
	// global route so it works no matter which window has focus
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
	{
		saveCurrentScene();
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("New"))
		{
			ImGui::MenuItem("New Scene", nullptr, &m_ShowNewSceneWindow);
			ImGui::EndMenu();
		}

		// lists every .json in the scenes folder, the current one gets a tick
		if (ImGui::BeginMenu("Scenes"))
		{
			// copy, not a reference, since loading a scene deletes the old one mid-loop
			const std::string currentScenePath = mp_sceneManager->getCurrentScene()->getScenePath();

			if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
			{
				saveCurrentScene();
			}
			ImGui::Separator();

			// error_code version so a missing folder doesn't throw
			std::error_code error;
			for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(kScenesDirectory, error))
			{
				if (entry.path().extension() != ".json")
				{
					continue;
				}

				const bool isCurrentScene = entry.path() == std::filesystem::path(currentScenePath);
				if (ImGui::MenuItem(entry.path().stem().string().c_str(), nullptr, isCurrentScene) && !isCurrentScene)
				{
					// Selected object belongs to the old scene, drop it before it's deleted
					mp_selectedGameObject = nullptr;

					if (mp_renderer->loadScene(entry.path().string()) != CRAIG_SUCCESS)
					{
						m_sceneLoadError = "Couldn't load " + entry.path().filename().string();
					}
					else
					{
						m_sceneLoadError.clear();
					}
				}
			}

			// show why the last load failed, if it did
			if (!m_sceneLoadError.empty())
			{
				ImGui::Separator();
				ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "%s", m_sceneLoadError.c_str());
			}

			ImGui::EndMenu();
		}

		// toggle which editor windows are open
		if (ImGui::BeginMenu("Windows"))
		{
			ImGui::MenuItem("Rendering Properties", nullptr, &m_ShowRendererProperties);
			ImGui::MenuItem("Scene Details", nullptr, &m_ShowSceneDetails);
			ImGui::MenuItem("New Game Object", nullptr, &m_ShowNewGameObjectWindow);
			ImGui::EndMenu();
		}

		// show the last save result for a few seconds
		constexpr double kSaveStatusDuration = 3.0;
		if (!m_saveStatus.empty() && ImGui::GetTime() - m_saveStatusTime < kSaveStatusDuration)
		{
			ImGui::Separator();
			ImGui::TextColored(m_saveStatusColour, "%s", m_saveStatus.c_str());
		}

		ImGui::EndMainMenuBar();
	}
}

void Craig::ImguiEditor::saveCurrentScene()
{
	const Craig::Scene* pScene = mp_sceneManager->getCurrentScene();
	const std::string fileName = std::filesystem::path(pScene->getScenePath()).filename().string();

	if (mp_sceneManager->getCurrentScene()->save() == CRAIG_SUCCESS)
	{
		m_saveStatus = "Saved " + fileName;
		m_saveStatusColour = { 0.4f, 1.0f, 0.4f, 1.0f };
	}
	else
	{
		m_saveStatus = "Couldn't save " + fileName;
		m_saveStatusColour = { 1.0f, 0.0f, 0.0f, 1.0f };
	}
	m_saveStatusTime = ImGui::GetTime();
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

		ImGui::SeparatorText("Physics");
		// Shown in Hz since that's easier to reason about, the physics engine stores it as seconds per step
		float physicsRate = 1.0f / mp_physicsEngine->getFixedTimeStep();
		if (ImGui::SliderFloat("Physics Rate", &physicsRate, 10.0f, 1000.0f, "%.0f Hz", ImGuiSliderFlags_AlwaysClamp)) {
			mp_physicsEngine->setFixedTimeStep(1.0f / physicsRate);
		}

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
	 	ImGui::Begin("Scene Details###SceneDetails", &m_ShowSceneDetails);

	 	if (ImGui::CollapsingHeader("Game Objects", ImGuiTreeNodeFlags_DefaultOpen))
	 	{

	 		if (ImGui::Button("New Gameobject")) {
	 			m_ShowNewGameObjectWindow = true;
	 		}

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

void Craig::ImguiEditor::renderNewSceneWindow()
{
	if (m_ShowNewSceneWindow)
	{
		// same centering as the new game object window
		const ImVec2 windowSize(500, 100);
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);
		const ImVec2 windowPos = ImVec2((mp_renderer->getWindowSize().x - windowSize.x) * 0.5f, (mp_renderer->getWindowSize().y - windowSize.y) * 0.5f);
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Appearing);

		ImGui::Begin("New Scene", &m_ShowNewSceneWindow);

		if (ImGui::InputText("Name", &m_newSceneName))
		{
			m_newSceneError.clear();
		}

		if (!m_newSceneError.empty())
		{
			ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "%s", m_newSceneError.c_str());
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::Button("Create"))
		{
			// The name doubles as the file name, so keep it to a single plain file
			const std::filesystem::path scenePath = std::filesystem::path(kScenesDirectory) / (m_newSceneName + ".json");

			if (m_newSceneName.empty())
			{
				m_newSceneError = "Scene must have a name.";
			}
			else if (m_newSceneName.find_first_of("/\\:*?\"<>|.") != std::string::npos)
			{
				m_newSceneError = "Name can't contain / \\ : * ? \" < > | or .";
			}
			else if (std::filesystem::exists(scenePath))
			{
				m_newSceneError = "A scene with that name already exists.";
			}
			else
			{
				// empty scene, just a camera at the origin and a sun so it isn't dark
				nlohmann::json sceneJson;
				sceneJson["name"] = m_newSceneName;

				nlohmann::json cameraJson = nlohmann::json::object();
				Utilities::writeJsonVec3(cameraJson, "position", glm::vec3(0.0f));
				Utilities::writeJsonVec2(cameraJson, "rotation", glm::vec2(0.0f));
				sceneJson["camera"] = cameraJson;

				nlohmann::json sunObjectJson;
				sunObjectJson["name"] = "Sun";
				sunObjectJson["components"]["sun"] = nlohmann::json::object(); // sun component fills in its defaults
				sceneJson["gameObjects"] = nlohmann::json::array({ sunObjectJson });

				std::ofstream sceneFile(scenePath);
				if (!sceneFile.is_open())
				{
					m_newSceneError = "Couldn't create " + scenePath.string();
				}
				else
				{
					sceneFile << sceneJson.dump(2) << std::endl;
					sceneFile.close();

					// Selected object belongs to the old scene, drop it before it's deleted
					mp_selectedGameObject = nullptr;

					if (mp_renderer->loadScene(scenePath.string()) != CRAIG_SUCCESS)
					{
						m_newSceneError = "Created the scene but couldn't load it.";
					}
					else
					{
						m_ShowNewSceneWindow = false;
					}
				}
			}
		}

		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			m_ShowNewSceneWindow = false;
		}

		ImGui::End();
	}

	if (!m_ShowNewSceneWindow)
	{
		m_newSceneName.clear();
		m_newSceneError.clear();
	}
}

void Craig::ImguiEditor::renderNewGameObjectWindow()
{
	if (m_ShowNewGameObjectWindow)
	{

		// Parameters for centering the window.
		const ImVec2 windowSize(500, 120);
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);
		const ImVec2 windowPos = ImVec2((mp_renderer->getWindowSize().x - windowSize.x) * 0.5f, (mp_renderer->getWindowSize().y - windowSize.y) * 0.5f);
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Appearing);

		// Create new game object window.
		ImGui::Begin("New Game Object", &m_ShowNewGameObjectWindow);

		// Draw input box for game object name.
		if (ImGui::InputText("Name", &m_newGameObjectName))
		{
			m_NewGameObjectError.clear();
		}

		ImGui::InputText("Path", &m_newGameObjectModelPath);
		ImGui::SameLine();
		if (ImGui::Button("Browse"))
		{
			m_modelBrowser.SetTitle("Select 3D Model");
			m_modelBrowser.SetDirectory("data/models");
			m_modelBrowser.SetTypeFilters({ ".glb" });
			m_modelBrowser.Open();

		}

		ImGui::TextDisabled("Leave the path empty for an empty game object");

		m_modelBrowser.Display();

		if(m_modelBrowser.HasSelected())
		{
			m_newGameObjectModelPath = std::filesystem::relative(m_modelBrowser.GetSelected(), std::filesystem::current_path()).string();
			m_modelBrowser.ClearSelected();
		}

		// If the game object name has any validation errors show them.
		if (!m_NewGameObjectError.empty())
		{
			ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, m_NewGameObjectError.c_str());
		}

		// Create on Enter or Create button press.
		if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::Button("Create"))
		{
			// Attempt to create the game object.
			const CraigError err = mp_renderer->newGameObject(m_newGameObjectName, m_newGameObjectModelPath, glm::vec3(0.0f, 0.0f, 0.0f));

			// Handle errors.
			switch (err)
			{
			case CRAIG_NO_NAME:
				m_NewGameObjectError = "Game object must have a name.";
				break;
			case CRAIG_DUPLICATE_NAME:
				m_NewGameObjectError = "Game object with that name already exists.";
				break;
			case CRAIG_FILE_NOT_FOUND:
				m_NewGameObjectError = "Couldn't find a file under that path";
				break;
			case CRAIG_FAIL:
				m_NewGameObjectError = "Only .glb models are supported";
				break;
			default:
				assert(err == CRAIG_SUCCESS);
				// Close the window.
				m_ShowNewGameObjectWindow = false;
				m_newGameObjectName.clear();
				m_NewGameObjectError.clear();
				break;
			}
		}

		// Render the close button.
		ImGui::SameLine();
		if (ImGui::Button("Close"))
		{
			m_ShowNewGameObjectWindow = false;
			m_newGameObjectName.clear();
			m_NewGameObjectError.clear();
		}

		ImGui::End();
	}

	if (!m_ShowNewGameObjectWindow)
	{
		m_newGameObjectName.clear();
		m_NewGameObjectError.clear();
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

		// ImGuizmo handles right-handed matrices now, so no LH hack needed
		// Proj rebuilt without the Vulkan Y-flip, ImGuizmo wants Y up
		const Craig::Camera& camera = mp_sceneManager->getCurrentScene()->getCamera();
		const glm::mat4 proj = glm::perspective(
			glm::radians(camera.m_fov), camera.m_aspect, camera.m_nearPlane, camera.m_farPlane);
		const glm::mat4 view = camera.getView();

		ImGuizmo::Manipulate(
			glm::value_ptr(view),
			glm::value_ptr(proj),
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

void Craig::ImguiEditor::updateImGuizmoBoxCollider()
{
	// Looked up fresh every frame instead of keeping the pointer around, so it can't dangle when the
	// collider/object gets removed or the scene changes. If more than one is selected the first one wins.
	mp_selectedBoxCollider = nullptr;
	for (Craig::GameObject* pGameObject : mp_sceneManager->getCurrentScene()->getGameObjects())
	{
		Craig::Components::BoxCollider* pCollider = pGameObject->getComponent<Craig::Components::BoxCollider>();
		if (pCollider != nullptr && pCollider->getSelected())
		{
			mp_selectedBoxCollider = pCollider;
			break;
		}
	}

	if (mp_selectedBoxCollider != nullptr)
	{
		// only one gizmo at a time, otherwise the object's and the collider's fight over the mouse
		mp_selectedGameObject = nullptr;

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

		// The collider's values are relative to its game object, but the gizmo works in world space.
		// So give it owner * local, then take the owner back out afterwards to get local again.
		const glm::mat4 ownerMatrix = mp_selectedBoxCollider->getOwner()->calculateModelMatrix();
		glm::mat4 transform = ownerMatrix * mp_selectedBoxCollider->getLocalMatrix();

		// ImGuizmo handles right-handed matrices now, so no LH hack needed
		// Proj rebuilt without the Vulkan Y-flip, ImGuizmo wants Y up
		const Craig::Camera& camera = mp_sceneManager->getCurrentScene()->getCamera();
		const glm::mat4 proj = glm::perspective(
			glm::radians(camera.m_fov), camera.m_aspect, camera.m_nearPlane, camera.m_farPlane);
		const glm::mat4 view = camera.getView();

		ImGuizmo::Manipulate(
			glm::value_ptr(view),
			glm::value_ptr(proj),
			m_CurrentOperation,
			ImGuizmo::MODE::LOCAL,
			glm::value_ptr(transform)
		);

		if (ImGuizmo::IsUsing())
		{
			// World -> local. If the owner has non-uniform scale and the collider is rotated this has shear in it,
			// which can't be split back into pos/rot/scale, so it'll come out slightly wrong in that case.
			const glm::mat4 local = glm::inverse(ownerMatrix) * transform;

			// Decompose the matrix manually so rotation stays as a quaternion (no Euler jumps).
			glm::vec3 pos = glm::vec3(local[3]);
			glm::vec3 scale = {
				glm::length(glm::vec3(local[0])),
				glm::length(glm::vec3(local[1])),
				glm::length(glm::vec3(local[2]))
			};
			glm::mat3 rotMat(
				glm::vec3(local[0]) / (scale.x != 0.0f ? scale.x : 1.0f),
				glm::vec3(local[1]) / (scale.y != 0.0f ? scale.y : 1.0f),
				glm::vec3(local[2]) / (scale.z != 0.0f ? scale.z : 1.0f)
			);
			glm::quat rot = glm::quat_cast(rotMat);

			switch (m_CurrentOperation)
			{
			case ImGuizmo::OPERATION::TRANSLATE:
				mp_selectedBoxCollider->setPosition(pos);
				break;
			case ImGuizmo::OPERATION::ROTATE:
				mp_selectedBoxCollider->setRotationQuat(rot);
				break;
			case ImGuizmo::OPERATION::SCALE:
				mp_selectedBoxCollider->setScale(scale);
				break;
			default:
				break;
			}
		}
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

// Draws a clip space line, cut off where it goes behind the camera
static void drawClippedLine(ImDrawList* pDrawList, glm::vec4 start, glm::vec4 end, const glm::vec2& screenSize, ImU32 colour)
{
	// Anything with w below this is (nearly) behind the camera, dividing by it would flip it across the screen
	constexpr float kMinW = 0.0001f;

	if (start.w < kMinW && end.w < kMinW)
	{
		return;
	}
	if (start.w < kMinW)
	{
		start = glm::mix(start, end, (kMinW - start.w) / (end.w - start.w));
	}
	else if (end.w < kMinW)
	{
		end = glm::mix(end, start, (kMinW - end.w) / (start.w - end.w));
	}

	pDrawList->AddLine(clipToScreen(start, screenSize), clipToScreen(end, screenSize), colour, 2.0f);
}

void Craig::ImguiEditor::drawBoxColliderOutlines()
{
	const Craig::Camera& camera = mp_sceneManager->getCurrentScene()->getCamera();
	const glm::mat4 proj = glm::perspective(
		glm::radians(camera.m_fov), camera.m_aspect, camera.m_nearPlane, camera.m_farPlane);
	const glm::mat4 viewProj = proj * camera.getView();
	const glm::vec2 screenSize = mp_renderer->getWindowSize();

	// background list draws over the scene but under the editor windows
	ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();

	for (Craig::GameObject* pGameObject : mp_sceneManager->getCurrentScene()->getGameObjects())
	{
		const Craig::Components::BoxCollider* pCollider = pGameObject->getComponent<Craig::Components::BoxCollider>();
		if (pCollider == nullptr || !pCollider->isOutlineVisible())
		{
			continue;
		}

		const ImU32 colour = pCollider == mp_selectedBoxCollider ? IM_COL32(255, 200, 0, 255) : IM_COL32(0, 255, 0, 255);
		const glm::mat4 mvp = viewProj * pCollider->getWorldMatrix();

		// Corner i uses bit 0 for x, bit 1 for y, bit 2 for z (0 = -0.5, 1 = +0.5)
		glm::vec4 corners[8];
		for (int i = 0; i < 8; i++)
		{
			const glm::vec3 localCorner = {
				(i & 1) ? 0.5f : -0.5f,
				(i & 2) ? 0.5f : -0.5f,
				(i & 4) ? 0.5f : -0.5f
			};
			corners[i] = mvp * glm::vec4(localCorner, 1.0f);
		}

		// Two corners share an edge if their indices only differ by one bit
		for (int a = 0; a < 8; a++)
		{
			for (int bit = 1; bit < 8; bit <<= 1)
			{
				const int b = a | bit;
				if (b == a)
				{
					continue; // Each edge once, from the corner with the bit unset
				}

				drawClippedLine(pDrawList, corners[a], corners[b], screenSize, colour);
			}
		}
	}
}

void Craig::ImguiEditor::updateImGuizmoSphereCollider()
{
	// same as the box one, looked up fresh every frame so it can't dangle
	mp_selectedSphereCollider = nullptr;
	for (Craig::GameObject* pGameObject : mp_sceneManager->getCurrentScene()->getGameObjects())
	{
		Craig::Components::SphereCollider* pCollider = pGameObject->getComponent<Craig::Components::SphereCollider>();
		if (pCollider != nullptr && pCollider->getSelected())
		{
			mp_selectedSphereCollider = pCollider;
			break;
		}
	}

	// Only one gizmo at a time, a selected box collider already has it
	if (mp_selectedSphereCollider == nullptr || mp_selectedBoxCollider != nullptr)
	{
		return;
	}

	mp_selectedGameObject = nullptr;

	// no rotate for spheres, it wouldn't do anything
	if (ImGui::IsKeyPressed(ImGuiKey_T))
	{
		m_CurrentOperation = ImGuizmo::TRANSLATE;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_E))
	{
		m_CurrentOperation = ImGuizmo::SCALE;
	}
	const ImGuizmo::OPERATION operation = m_CurrentOperation == ImGuizmo::SCALE ? ImGuizmo::SCALE : ImGuizmo::TRANSLATE;

	ImGuizmo::SetOrthographic(false);
	const glm::vec2 screenSize = mp_renderer->getWindowSize();
	ImGuizmo::SetRect(0, 0, screenSize.x, screenSize.y);

	// Built in world space from the centre and radius, with the owner's rotation so the handles line up with the object
	Craig::GameObject* pOwner = mp_selectedSphereCollider->getOwner();
	const float worldRadius = mp_selectedSphereCollider->getWorldRadius();
	glm::mat4 transform = glm::translate(glm::mat4(1.0f), mp_selectedSphereCollider->getWorldCentre())
		* glm::mat4_cast(pOwner->getRotationQuat())
		* glm::scale(glm::mat4(1.0f), glm::vec3(worldRadius));

	const Craig::Camera& camera = mp_sceneManager->getCurrentScene()->getCamera();
	const glm::mat4 proj = glm::perspective(
		glm::radians(camera.m_fov), camera.m_aspect, camera.m_nearPlane, camera.m_farPlane);
	const glm::mat4 view = camera.getView();

	ImGuizmo::Manipulate(
		glm::value_ptr(view),
		glm::value_ptr(proj),
		operation,
		ImGuizmo::MODE::LOCAL,
		glm::value_ptr(transform)
	);

	if (!ImGuizmo::IsUsing())
	{
		return;
	}

	if (operation == ImGuizmo::TRANSLATE)
	{
		// World -> the owner's space, which is what the collider's position is in
		const glm::vec3 worldCentre = glm::vec3(transform[3]);
		mp_selectedSphereCollider->setPosition(glm::vec3(glm::inverse(pOwner->calculateModelMatrix()) * glm::vec4(worldCentre, 1.0f)));
	}
	else
	{
		// Dragging one axis only scales that axis, so go with whichever one changed the most
		float bestRatio = 1.0f;
		for (int axis = 0; axis < 3; axis++)
		{
			const float ratio = glm::length(glm::vec3(transform[axis])) / worldRadius;
			if (glm::abs(ratio - 1.0f) > glm::abs(bestRatio - 1.0f))
			{
				bestRatio = ratio;
			}
		}
		mp_selectedSphereCollider->setRadius(mp_selectedSphereCollider->getRadius() * bestRatio);
	}
}

void Craig::ImguiEditor::drawSphereColliderOutlines()
{
	const Craig::Camera& camera = mp_sceneManager->getCurrentScene()->getCamera();
	const glm::mat4 proj = glm::perspective(
		glm::radians(camera.m_fov), camera.m_aspect, camera.m_nearPlane, camera.m_farPlane);
	const glm::mat4 viewProj = proj * camera.getView();
	const glm::vec2 screenSize = mp_renderer->getWindowSize();

	ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();

	constexpr int kSegments = 48;

	for (Craig::GameObject* pGameObject : mp_sceneManager->getCurrentScene()->getGameObjects())
	{
		const Craig::Components::SphereCollider* pCollider = pGameObject->getComponent<Craig::Components::SphereCollider>();
		if (pCollider == nullptr || !pCollider->isOutlineVisible())
		{
			continue;
		}

		const ImU32 colour = pCollider == mp_selectedSphereCollider ? IM_COL32(255, 200, 0, 255) : IM_COL32(0, 255, 0, 255);
		const glm::vec3 centre = pCollider->getWorldCentre();
		const float radius = pCollider->getWorldRadius();

		// a circle around each world axis, that's the shape physics actually uses
		for (int axis = 0; axis < 3; axis++)
		{
			// the two axes the circle lies in
			glm::vec3 u(0.0f), v(0.0f);
			u[(axis + 1) % 3] = radius;
			v[(axis + 2) % 3] = radius;

			glm::vec4 previous = viewProj * glm::vec4(centre + u, 1.0f);
			for (int i = 1; i <= kSegments; i++)
			{
				const float angle = glm::two_pi<float>() * static_cast<float>(i) / kSegments;
				const glm::vec4 current = viewProj * glm::vec4(centre + u * glm::cos(angle) + v * glm::sin(angle), 1.0f);
				drawClippedLine(pDrawList, previous, current, screenSize, colour);
				previous = current;
			}
		}
	}
}
