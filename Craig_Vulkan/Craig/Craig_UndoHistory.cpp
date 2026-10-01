#include "Craig_UndoHistory.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_SceneManager.hpp"
#include "Craig_Scene.hpp"
#include "Craig_GameObject.hpp"
#include "Craig_Logger.hpp"

#include "../External/Imgui/imgui.h"
#include "../External/Imgui/ImGuizmo/ImGuizmo.h"

// plenty, and each step is only the objects that changed
constexpr size_t kMaxUndoSteps = 100;

void Craig::UndoHistory::init(Craig::Renderer* pRenderer, Craig::SceneManager* pSceneManager) {
	mp_renderer = pRenderer;
	mp_sceneManager = pSceneManager;
}

void Craig::UndoHistory::update(bool isEditing) {

	// playing changes everything every frame, none of that's an edit
	// Stop puts the scene back exactly, so the history's still good after
	if (!isEditing)
	{
		m_hasBaseline = false;
		m_wasBusy = false;
		m_framesToScan = 0;
		return;
	}

	// different scene, old history means nothing here
	const std::string& scenePath = mp_sceneManager->getCurrentScene()->getScenePath();
	if (scenePath != m_scenePath)
	{
		clear();
		m_scenePath = scenePath;
	}

	// first frame back, just remember how things are
	if (!m_hasBaseline)
	{
		m_baseline = captureState();
		m_hasBaseline = true;
	}

	// holding a widget or the gizmo, wait till they let go
	const bool busy = ImGui::IsAnyItemActive() || ImGuizmo::IsUsing();
	if (m_wasBusy && !busy)
	{
		m_framesToScan = 2;
	}
	m_wasBusy = busy;

	if (m_framesToScan > 0 && !busy)
	{
		scan();
		m_framesToScan--;
	}
}

bool Craig::UndoHistory::undo() {

	// anything not picked up yet counts as the latest step
	scan();

	if (m_undoStack.empty())
	{
		return false;
	}

	Entry entry = std::move(m_undoStack.back());
	m_undoStack.pop_back();

	applyEntry(entry, true);
	Craig::Logger::scene().info("Undo: {}", describe(entry));

	m_redoStack.push_back(std::move(entry));
	return true;
}

bool Craig::UndoHistory::redo() {

	// a new edit kills the redo stack, same as everywhere else
	scan();

	if (m_redoStack.empty())
	{
		return false;
	}

	Entry entry = std::move(m_redoStack.back());
	m_redoStack.pop_back();

	applyEntry(entry, false);
	Craig::Logger::scene().info("Redo: {}", describe(entry));

	m_undoStack.push_back(std::move(entry));
	return true;
}

void Craig::UndoHistory::clear() {
	m_undoStack.clear();
	m_redoStack.clear();
	m_hasBaseline = false;
}

Craig::UndoHistory::SceneState Craig::UndoHistory::captureState() const {

	SceneState state;
	Craig::Scene* pScene = mp_sceneManager->getCurrentScene();
	for (const Craig::GameObject* pObject : pScene->getGameObjects())
	{
		state[pObject->getName()] = pScene->gameObjectToJson(pObject);
	}
	return state;
}

void Craig::UndoHistory::scan() {

	if (!m_hasBaseline)
	{
		return;
	}

	SceneState current = captureState();

	// changed or deleted
	Entry entry;
	for (const auto& [name, beforeJson] : m_baseline)
	{
		const SceneState::const_iterator it = current.find(name);
		if (it == current.end())
		{
			entry.push_back({ name, beforeJson, nullptr });
		}
		else if (it->second != beforeJson)
		{
			entry.push_back({ name, beforeJson, it->second });
		}
	}

	// new ones
	for (const auto& [name, afterJson] : current)
	{
		if (!m_baseline.contains(name))
		{
			entry.push_back({ name, nullptr, afterJson });
		}
	}

	m_baseline = std::move(current);

	// clicked something that doesn't touch the scene (vsync, log filter...)
	if (entry.empty())
	{
		return;
	}

	Craig::Logger::scene().debug("Recorded undo step: {}", describe(entry));

	m_undoStack.push_back(std::move(entry));
	if (m_undoStack.size() > kMaxUndoSteps)
	{
		m_undoStack.pop_front();
	}
	m_redoStack.clear();
}

void Craig::UndoHistory::applyEntry(const Entry& entry, bool useBefore) {

	Craig::Scene* pScene = mp_sceneManager->getCurrentScene();

	// everything goes first, then gets remade from json
	// that way renames and the one sun rule don't trip over the old copy
	for (const ObjectChange& change : entry)
	{
		if (Craig::GameObject* pObject = pScene->findObject(change.name))
		{
			mp_renderer->deleteGameObject(pObject);
		}
	}

	for (const ObjectChange& change : entry)
	{
		const nlohmann::json& objectJson = useBefore ? change.before : change.after;
		if (!objectJson.is_null())
		{
			pScene->createGameObjectFromJson(objectJson);
		}
	}

	// otherwise the next scan thinks the undo was an edit
	m_baseline = captureState();
}

std::string Craig::UndoHistory::describe(const Entry& entry) {

	std::string text;
	for (const ObjectChange& change : entry)
	{
		if (!text.empty())
		{
			text += ", ";
		}

		if (change.before.is_null())
		{
			text += "added '" + change.name + "'";
		}
		else if (change.after.is_null())
		{
			text += "deleted '" + change.name + "'";
		}
		else
		{
			text += "changed '" + change.name + "'";
		}
	}
	return text;
}
