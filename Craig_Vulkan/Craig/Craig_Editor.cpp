#include "Craig_Editor.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_Camera.hpp"
#include "Craig_PhysicsEngine.hpp"
#include "Craig_Game.hpp"

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

#include "Components/Craig_Collider.hpp"

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
		ImGui::DockBuilderDockWindow("###Log", dock_id_bottom);
		ImGui::DockBuilderFinish(dockspace_id);

		//Default windows to open
		m_ShowRendererProperties = true;
		m_ShowSceneDetails = true;
		m_ShowLog = true;

		//When we initialise the renderer we have the max sampling level set, so for now this is good enough since we change it in both places at once
		//TODO: Keep track of the current level in the renderer, not both there and here
		m_currentMSAALevel = (int)mp_renderer->getRenderingAttachments().getMaxSamplingLevel();
		mv_MSAADropdownOptions.resize(m_MSAAIndexes[mp_renderer->getRenderingAttachments().getMaxSamplingLevel()] + 1);
		m_MSAADropdownIndex = m_MSAAIndexes[m_currentMSAALevel];

		m_undoHistory.init(mp_renderer, mp_sceneManager);

		m_initialised = true;
	}
	


	return ret;
}


CraigError Craig::ImguiEditor::editorMain(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

	// menu bar goes first so the dockspace fits underneath it
	showMainMenuBar();
	editorInit();
	// before the windows so nothing's halfway through the object list when stuff gets deleted
	handleShortcuts();

	showRenderProperties(deltaTime);
	showSceneDetails(deltaTime);
	showLog();
	updateImGuizmo();
	updateImGuizmoCollider();
	drawColliderOutlines();

	// making a scene loads it, which would screw up the play snapshot
	// no new game objects either
	if (isEditing())
	{
		renderNewGameObjectWindow();
		renderNewSceneWindow();
	}

	// last, everything that could edit the scene has had its go this frame
	m_undoHistory.update(isEditing());

	// game's stand in menus, still drawn while paused
	if (mp_engineModes != nullptr)
	{
		const Craig::EngineContext& modeContext = mp_engineModes->getContext();
		if (modeContext.pGame != nullptr && modeContext.gameSessionActive)
		{
			modeContext.pGame->drawImGui();
		}
	}

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

	// play/stop toggle
	if (mp_engineModes != nullptr && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P, ImGuiInputFlags_RouteGlobal))
	{
		mp_engineModes->requestChange(isEditing() ? Craig::EngineModeId::Play : Craig::EngineModeId::Edit);
	}

	// orange menu bar while playing, so you know whatever you change gets binned on Stop
	const bool tintMenuBar = !isEditing();
	if (tintMenuBar)
	{
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.45f, 0.25f, 0.05f, 1.0f));
	}

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("New"))
		{
			ImGui::MenuItem("New Scene", nullptr, &m_ShowNewSceneWindow, isEditing());
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, isEditing() && m_undoHistory.canUndo()))
			{
				undoOrRedo(true);
			}
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, isEditing() && m_undoHistory.canRedo()))
			{
				undoOrRedo(false);
			}
			ImGui::Separator();

			const bool hasObject = mp_selectedGameObject != nullptr;
			const bool hasSelection = hasObject || mp_selectedCollider != nullptr;
			if (ImGui::MenuItem("Deselect", "Ctrl+D", false, hasSelection))
			{
				selectGameObject(nullptr); // drops colliders too
			}
			if (ImGui::MenuItem("Focus Selected", "F", false, hasSelection))
			{
				focusOnSelected();
			}
			if (ImGui::MenuItem("Duplicate", "Ctrl+Shift+D", false, isEditing() && hasObject))
			{
				duplicateSelectedGameObject();
			}
			if (ImGui::MenuItem("Delete", "Delete", false, isEditing() && hasSelection))
			{
				deleteSelected();
			}
			ImGui::Separator();
			ImGui::MenuItem("New Game Object", "Ctrl+N", &m_ShowNewGameObjectWindow, isEditing());
			ImGui::EndMenu();
		}

		// lists every .json in the scenes folder, the current one gets a tick
		if (ImGui::BeginMenu("Scenes"))
		{
			// copy, not a reference, since loading a scene deletes the old one mid-loop
			const std::string currentScenePath = mp_sceneManager->getCurrentScene()->getScenePath();

			if (ImGui::MenuItem("Save Scene", "Ctrl+S", false, isEditing()))
			{
				saveCurrentScene();
			}
			ImGui::Separator();

			// switching mid play would leave Stop putting back a scene you'd already left
			ImGui::BeginDisabled(!isEditing());

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

			ImGui::EndDisabled();

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
			ImGui::MenuItem("Log", nullptr, &m_ShowLog);
			ImGui::MenuItem("New Game Object", nullptr, &m_ShowNewGameObjectWindow, isEditing());
			ImGui::EndMenu();
		}

		showPlayControls();

		// show the last save result for a few seconds
		constexpr double kSaveStatusDuration = 3.0;
		if (!m_saveStatus.empty() && ImGui::GetTime() - m_saveStatusTime < kSaveStatusDuration)
		{
			ImGui::Separator();
			ImGui::TextColored(m_saveStatusColour, "%s", m_saveStatus.c_str());
		}

		ImGui::EndMainMenuBar();
	}

	if (tintMenuBar)
	{
		ImGui::PopStyleColor();
	}
}

void Craig::ImguiEditor::showPlayControls()
{
	if (mp_engineModes == nullptr)
	{
		return;
	}

	ImGui::Separator();

	// greyed out if the transition table says no
	// new modes only need a line here
	struct ModeButton {
		const char* label;
		Craig::EngineModeId target;
		const char* tooltip;
	};
	static constexpr ModeButton kButtons[] = {
		{ "Play", Craig::EngineModeId::Play, "Run the game (Ctrl+P)" },
		{ "Pause", Craig::EngineModeId::Pause, "Freeze it, you can look around but not edit" },
		{ "Step", Craig::EngineModeId::Step, "One frame (one physics step) then pause again" },
		{ "Simulate", Craig::EngineModeId::Simulate, "Physics only, no gameplay" },
		{ "Stop", Craig::EngineModeId::Edit, "Back to editing, puts the scene back how it was (Ctrl+P)" },
	};

	for (const ModeButton& button : kButtons)
	{
		ImGui::BeginDisabled(!mp_engineModes->canChangeTo(button.target));
		if (ImGui::MenuItem(button.label))
		{
			mp_engineModes->requestChange(button.target);
		}
		ImGui::EndDisabled();
		ImGui::SetItemTooltip("%s", button.tooltip);
	}

	ImGui::Separator();
	ImGui::TextDisabled("%s", mp_engineModes->getCurrent()->getName());
}

bool Craig::ImguiEditor::isEditing() const
{
	// no modes hooked up = never playing
	return mp_engineModes == nullptr || mp_engineModes->isCurrent(Craig::EngineModeId::Edit);
}

void Craig::ImguiEditor::saveCurrentScene()
{
	// Ctrl+S still lands here mid play
	// saving now would write the played positions over the real ones
	if (!isEditing())
	{
		Craig::Logger::scene().warn("Not saving while playing, hit Stop first");
		m_saveStatus = "Stop playing before saving";
		m_saveStatusColour = { 1.0f, 0.0f, 0.0f, 1.0f };
		m_saveStatusTime = ImGui::GetTime();
		return;
	}

	const Craig::Scene* pScene = mp_sceneManager->getCurrentScene();
	const std::string fileName = std::filesystem::path(pScene->getScenePath()).filename().string();

	if (mp_sceneManager->getCurrentScene()->save() == CRAIG_SUCCESS)
	{
		// undo only goes back as far as the last save
		m_undoHistory.clear();

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

// the camera's forward() is backwards and private, the view matrix knows which way it's actually looking
static glm::vec3 getCameraLookDir(const Craig::Camera& camera)
{
	return -glm::normalize(glm::vec3(glm::inverse(camera.getView())[2]));
}

void Craig::ImguiEditor::handleShortcuts()
{
	// typing a name with an f in it shouldn't fling the camera across the map
	const bool typing = ImGui::GetIO().WantTextInput;

	// text boxes grab these first while you're typing in one, so they keep their own undo
	constexpr ImGuiInputFlags kUndoFlags = ImGuiInputFlags_RouteGlobal | ImGuiInputFlags_Repeat;
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, kUndoFlags))
	{
		undoOrRedo(true);
	}
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, kUndoFlags) || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, kUndoFlags))
	{
		undoOrRedo(false);
	}
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, ImGuiInputFlags_RouteGlobal))
	{
		selectGameObject(nullptr); // drops colliders too
	}
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_D, ImGuiInputFlags_RouteGlobal))
	{
		duplicateSelectedGameObject();
	}
	if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, ImGuiInputFlags_RouteGlobal) && isEditing())
	{
		m_ShowNewGameObjectWindow = true;
	}
	if (!typing && ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteGlobal))
	{
		deleteSelected();
	}
	if (!typing && ImGui::Shortcut(ImGuiKey_F, ImGuiInputFlags_RouteGlobal))
	{
		focusOnSelected();
	}

	// scroll to move forwards/back, only over the scene not the editor windows
	const float wheel = ImGui::GetIO().MouseWheel;
	if (isEditing() && wheel != 0.0f && !ImGui::GetIO().WantCaptureMouse)
	{
		Craig::Camera& camera = mp_sceneManager->getCurrentScene()->getCamera();
		// 1 unit a notch at the default speed, so the move speed slider affects it too
		const float step = wheel * camera.m_movementSpeed * 0.1f;
		camera.setPosition(camera.getPosition() + getCameraLookDir(camera) * step);
	}
}

void Craig::ImguiEditor::undoOrRedo(bool undo)
{
	// ctrl z halfway through dragging something would be a mess
	if (!isEditing() || ImGui::IsAnyItemActive() || ImGuizmo::IsUsing())
	{
		return;
	}

	// whatever's selected might get deleted and remade, so go by name
	const std::string selectedName = getSelectedGameObjectName();
	const bool changed = undo ? m_undoHistory.undo() : m_undoHistory.redo();
	if (changed)
	{
		onSceneSwapped(selectedName);
	}
}

void Craig::ImguiEditor::deleteSelected()
{
	if (!isEditing())
	{
		return;
	}

	if (mp_selectedCollider != nullptr)
	{
		Craig::Components::Collider* pCollider = mp_selectedCollider;
		deselectAllColliders();
		Craig::Logger::scene().info("Removed {} from '{}'", pCollider->getTypeName(), pCollider->getOwner()->getName());
		pCollider->getOwner()->removeComponent(pCollider);
	}
	else if (mp_selectedGameObject != nullptr)
	{
		Craig::GameObject* pGameObject = mp_selectedGameObject;
		mp_selectedGameObject = nullptr;
		mp_renderer->deleteGameObject(pGameObject);
	}
	else
	{
		return;
	}

	// not a widget so undo wouldn't notice otherwise
	m_undoHistory.scanSoon();
}

void Craig::ImguiEditor::duplicateSelectedGameObject()
{
	if (!isEditing() || mp_selectedGameObject == nullptr)
	{
		return;
	}

	Craig::Scene* pScene = mp_sceneManager->getCurrentScene();

	// first free "Fish (1)", "Fish (2)"...
	std::string newName;
	for (int i = 1; newName.empty() || pScene->findObject(newName) != nullptr; i++)
	{
		newName = mp_selectedGameObject->getName() + " (" + std::to_string(i) + ")";
	}

	// same json it'd save with, just a new name
	nlohmann::json objectJson = pScene->gameObjectToJson(mp_selectedGameObject);
	objectJson["name"] = newName;

	Craig::GameObject* pCopy = pScene->createGameObjectFromJson(objectJson);
	if (pCopy == nullptr)
	{
		return;
	}

	Craig::Logger::scene().info("Duplicated '{}' as '{}'", mp_selectedGameObject->getName(), newName);
	selectGameObject(pCopy);
	m_undoHistory.scanSoon();
}

void Craig::ImguiEditor::focusOnSelected()
{
	glm::vec3 target;
	float size = 1.0f;
	if (mp_selectedCollider != nullptr)
	{
		target = glm::vec3(mp_selectedCollider->getGizmoMatrix()[3]);
	}
	else if (mp_selectedGameObject != nullptr)
	{
		target = mp_selectedGameObject->getPosition();
		const glm::vec3& scale = mp_selectedGameObject->getScale();
		size = glm::max(scale.x, glm::max(scale.y, scale.z));
	}
	else
	{
		return;
	}

	// keep looking the same way, just back off from the target along it
	// no bounds on models yet so the distance is a guess off the scale
	Craig::Camera& camera = mp_sceneManager->getCurrentScene()->getCamera();
	const float distance = glm::max(3.0f, size * 5.0f);
	camera.setPosition(target - getCameraLookDir(camera) * distance);
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
			Craig::Logger::renderer().info("VSync {}", mp_renderer->getVSyncState() ? "on" : "off");
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

// Colour for each log level, same order as spdlog's level_enum
static ImVec4 getLogLevelColour(spdlog::level::level_enum level)
{
	switch (level)
	{
	case spdlog::level::trace:		return { 0.5f, 0.5f, 0.5f, 1.0f };
	case spdlog::level::debug:		return { 0.6f, 0.7f, 0.8f, 1.0f };
	case spdlog::level::warn:		return { 1.0f, 0.8f, 0.2f, 1.0f };
	case spdlog::level::err:		return { 1.0f, 0.4f, 0.4f, 1.0f };
	case spdlog::level::critical:	return { 1.0f, 0.0f, 0.0f, 1.0f };
	default:						return ImGui::GetStyleColorVec4(ImGuiCol_Text);
	}
}

void Craig::ImguiEditor::showLog()
{
	if (m_ShowLog)
	{
		// ### for a unique ID so it stays docked at the bottom
		ImGui::Begin("Log###Log", &m_ShowLog);

		const std::shared_ptr<Craig::EditorLogSink> pSink = Craig::Logger::getInstance().getEditorSink();
		bool rebuildVisibleLines = false;

		if (ImGui::Button("Clear")) {
			pSink->clear();
		}
		ImGui::SameLine();
		const bool copyPressed = ImGui::Button("Copy");
		ImGui::SameLine();
		ImGui::Checkbox("Auto-scroll", &m_logAutoScroll);
		ImGui::SameLine();

		constexpr const char* kLogLevelNames[] = { "Trace", "Debug", "Info", "Warn", "Error", "Critical" };
		ImGui::SetNextItemWidth(100.0f);
		if (ImGui::Combo("Level", &m_logMinLevel, kLogLevelNames, IM_ARRAYSIZE(kLogLevelNames))) {
			rebuildVisibleLines = true;
		}
		ImGui::SameLine();
		if (m_logFilter.Draw("Filter", 200.0f)) {
			rebuildVisibleLines = true;
		}

		// only copy when something's actually been logged, not every frame
		const uint64_t sinkVersion = pSink->getVersion();
		if (sinkVersion != m_logVersion) {
			mv_logEntries = pSink->getEntries();
			m_logVersion = sinkVersion;
			rebuildVisibleLines = true;
		}

		if (rebuildVisibleLines) {
			mv_visibleLogLines.clear();
			for (int i = 0; i < static_cast<int>(mv_logEntries.size()); i++) {
				const Craig::LogEntry& entry = mv_logEntries[i];
				if (entry.level >= m_logMinLevel && m_logFilter.PassFilter(entry.text.c_str())) {
					mv_visibleLogLines.push_back(i);
				}
			}
		}

		// copies what you can see (level and filter applied)
		if (copyPressed) {
			std::string clipboardText;
			for (int line : mv_visibleLogLines) {
				clipboardText += mv_logEntries[line].text;
				clipboardText += '\n';
			}
			ImGui::SetClipboardText(clipboardText.c_str());
		}

		ImGui::Separator();

		if (ImGui::BeginChild("LogScrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
		{
			// clipper only draws what's on screen, all 5000 lines every frame would tank the fps
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(mv_visibleLogLines.size()));
			while (clipper.Step()) {
				for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; line++) {
					const Craig::LogEntry& entry = mv_logEntries[mv_visibleLogLines[line]];
					ImGui::PushStyleColor(ImGuiCol_Text, getLogLevelColour(entry.level));
					ImGui::TextUnformatted(entry.text.c_str());
					ImGui::PopStyleColor();
				}
			}
			clipper.End();

			// stick to the bottom, unless you've scrolled up to read something
			if (m_logAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}
		}
		ImGui::EndChild();

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

	 		// nothing can change outside edit mode
	 		// you can still select stuff and look at it
	 		const bool editable = isEditing();
	 		if (!editable)
	 		{
	 			ImGui::TextDisabled("Playing, hit Stop to edit");
	 		}

	 		ImGui::BeginDisabled(!editable);
	 		if (ImGui::Button("New Gameobject")) {
	 			m_ShowNewGameObjectWindow = true;
	 		}
	 		ImGui::EndDisabled();

	 		// Display all properties of game objects in the scene.
	 		const std::vector<Craig::GameObject*>& gameOjects = mp_sceneManager->getCurrentScene()->getGameObjects();
	 		for (Craig::GameObject* pGameObject : gameOjects)
	 		{
	 			// Separate the list a little for visibility
	 			ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 4.0f);

	 			// We have to push a different ID to each node as we're using the same ID otherwise.
	 			// name not pointer, undo remakes the object and a new pointer = node closes on you
	 			ImGui::PushID(pGameObject->getName().c_str());
	 			if (ImGui::TreeNode("##TreeNode", "%s", pGameObject->getName().c_str()))
	 			{
	 				// Allow the user to select the game object.
	 				if ((pGameObject == nullptr || pGameObject != mp_selectedGameObject) && ImGui::Button("Select"))
	 				{
	 					selectGameObject(pGameObject);
	 				}

	 				if (mp_selectedGameObject != nullptr && pGameObject == mp_selectedGameObject)
	 				{
	 					// Allow the user to deselect the game object.
	 					if (ImGui::Button("Deselect"))
	 					{
	 						mp_selectedGameObject = nullptr;
	 					}

	 					ImGui::BeginDisabled(!editable);
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
	 					ImGui::EndDisabled();
	 				}

	 				// Allow the user to delete the game object.
	 				ImGui::BeginDisabled(!editable);
	 				const bool deletePressed = ImGui::Button("Delete Object");
	 				ImGui::EndDisabled();
	 				if (deletePressed)
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
	 				pGameObject->displayImGuiAttributes(editable);

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
		const ImVec2 windowPos = ImVec2((ImGui::GetIO().DisplaySize.x - windowSize.x) * 0.5f, (ImGui::GetIO().DisplaySize.y - windowSize.y) * 0.5f);
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
					Craig::Logger::scene().error("Couldn't create {}", scenePath.string());
					m_newSceneError = "Couldn't create " + scenePath.string();
				}
				else
				{
					sceneFile << sceneJson.dump(2) << std::endl;
					sceneFile.close();
					Craig::Logger::scene().info("Created new scene '{}' at {}", m_newSceneName, scenePath.string());

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
		const ImVec2 windowPos = ImVec2((ImGui::GetIO().DisplaySize.x - windowSize.x) * 0.5f, (ImGui::GetIO().DisplaySize.y - windowSize.y) * 0.5f);
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
			ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "%s", m_NewGameObjectError.c_str());
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

	// no gizmo while playing
	if (mp_selectedGameObject != nullptr && isEditing())
	{
		// Use hotkeys to update the current transformation.
		handleGizmoHotkeys(true);

		// Set the screen rect and tell ImGuizmo how to project.
		ImGuizmo::SetOrthographic(false);
		// ImGui's size, not the swapchain's, they're different on high DPI screens
		const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
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

void Craig::ImguiEditor::selectGameObject(Craig::GameObject* pGameObject)
{
	deselectAllColliders();
	mp_selectedGameObject = pGameObject;
}

std::string Craig::ImguiEditor::getSelectedGameObjectName() const
{
	return mp_selectedGameObject != nullptr ? mp_selectedGameObject->getName() : "";
}

void Craig::ImguiEditor::onSceneSwapped(const std::string& reselectObjectName)
{
	// old pointers are dead, just forget them
	mp_selectedGameObject = nullptr;
	mp_selectedCollider = nullptr;

	Craig::GameObject* pReselect = reselectObjectName.empty() ? nullptr : mp_sceneManager->getCurrentScene()->findObject(reselectObjectName);
	selectGameObject(pReselect);
}

void Craig::ImguiEditor::selectCollider(Craig::Components::Collider* pCollider)
{
	mp_selectedGameObject = nullptr;
	deselectAllColliders();
	pCollider->setSelected(true);
}

void Craig::ImguiEditor::deselectAllColliders()
{
	for (Craig::GameObject* pGameObject : mp_sceneManager->getCurrentScene()->getGameObjects())
	{
		for (Craig::Components::Collider* pCollider : pGameObject->getComponents<Craig::Components::Collider>())
		{
			pCollider->setSelected(false);
		}
	}
	mp_selectedCollider = nullptr;
}

void Craig::ImguiEditor::handleGizmoHotkeys(bool canRotate)
{
	// otherwise typing "tree" in a name box flips through every mode
	if (ImGui::GetIO().WantTextInput)
	{
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_T))
	{
		m_CurrentOperation = ImGuizmo::TRANSLATE;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_R) && canRotate)
	{
		m_CurrentOperation = ImGuizmo::ROTATE;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_E))
	{
		m_CurrentOperation = ImGuizmo::SCALE;
	}
}

void Craig::ImguiEditor::updateImGuizmoCollider()
{
	// Looked up fresh every frame instead of keeping the pointer around, so it can't dangle when the
	// collider/object gets removed or the scene changes. selectCollider makes sure there's only ever one.
	mp_selectedCollider = nullptr;
	for (Craig::GameObject* pGameObject : mp_sceneManager->getCurrentScene()->getGameObjects())
	{
		for (Craig::Components::Collider* pCollider : pGameObject->getComponents<Craig::Components::Collider>())
		{
			if (pCollider->getSelected())
			{
				mp_selectedCollider = pCollider;
				break;
			}
		}
		if (mp_selectedCollider != nullptr)
		{
			break;
		}
	}

	// still found above so the outline stays highlighted, just no gizmo
	if (mp_selectedCollider == nullptr || !isEditing())
	{
		return;
	}

	// Use hotkeys to update the current transformation.
	handleGizmoHotkeys(mp_selectedCollider->canRotate());
	// Still in rotate from something else, fall back to move for shapes that can't rotate
	const ImGuizmo::OPERATION operation = (m_CurrentOperation == ImGuizmo::ROTATE && !mp_selectedCollider->canRotate())
		? ImGuizmo::TRANSLATE : m_CurrentOperation;

	// Set the screen rect and tell ImGuizmo how to project.
	ImGuizmo::SetOrthographic(false);
	// ImGui's size, not the swapchain's, they're different on high DPI screens
	const ImVec2 screenSize = ImGui::GetIO().DisplaySize;
	ImGuizmo::SetRect(0, 0, screenSize.x, screenSize.y);

	// Each collider builds its own world space gizmo matrix and takes it back apart afterwards
	glm::mat4 transform = mp_selectedCollider->getGizmoMatrix();

	// ImGuizmo handles right-handed matrices now, so no LH hack needed
	// Proj rebuilt without the Vulkan Y-flip, ImGuizmo wants Y up
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

	if (ImGuizmo::IsUsing())
	{
		mp_selectedCollider->applyGizmoMatrix(transform, operation);
	}
}

void Craig::ImguiEditor::drawColliderOutlines()
{
	const Craig::Camera& camera = mp_sceneManager->getCurrentScene()->getCamera();
	const glm::mat4 proj = glm::perspective(
		glm::radians(camera.m_fov), camera.m_aspect, camera.m_nearPlane, camera.m_farPlane);

	Craig::Components::ColliderOutlineContext context;
	// background list draws over the scene but under the editor windows
	context.pDrawList = ImGui::GetBackgroundDrawList();
	context.viewProj = proj * camera.getView();
	context.screenSize = glm::vec2(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y); // outlines are drawn with imgui so they use its size
	// The camera's position is the inverse view's translation (the camera getter isn't const)
	context.cameraPos = glm::vec3(glm::inverse(camera.getView())[3]);

	for (Craig::GameObject* pGameObject : mp_sceneManager->getCurrentScene()->getGameObjects())
	{
		for (const Craig::Components::Collider* pCollider : pGameObject->getComponents<Craig::Components::Collider>())
		{
			if (!pCollider->isOutlineVisible())
			{
				continue;
			}

			context.colour = pCollider == mp_selectedCollider ? IM_COL32(255, 200, 0, 255) : IM_COL32(0, 255, 0, 255);
			pCollider->drawOutline(context);
		}
	}
}
