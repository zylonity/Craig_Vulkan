#include "Craig_Model.hpp"
#include "Craig/Craig_GameObject.hpp"
#include "Craig/Craig_ResourceManager.hpp"
#include "Craig/Craig_Scene.hpp"
#include "Craig/Craig_Logger.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"

#include <filesystem>

CraigError Craig::Components::Model::init() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Components::Model::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::Components::Model::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	// Our geometry is still in the vertex/index buffers, get them rebuilt without it
	if (hasModel())
	{
		markSceneGeometryDirty();
	}

	return ret;
}

CraigError Craig::Components::Model::loadFromJson(const nlohmann::json& json) {

	return setModelPath(json.value("path", ""));
}

void Craig::Components::Model::saveToJson(nlohmann::json& json) const {

	json["path"] = m_modelPath;
}

CraigError Craig::Components::Model::setModelPath(const std::string& modelPath) {

	CraigError ret = CRAIG_SUCCESS;

	if (modelPath == m_modelPath)
	{
		return ret;
	}

	if (!modelPath.empty())
	{
		if (!std::filesystem::exists(modelPath))
		{
			Craig::Logger::resources().warn("'{}' can't use {}, the file doesn't exist", mp_owner->getName(), modelPath);
			return CRAIG_FILE_NOT_FOUND;
		}

		// ResourceManager only does binary glTF and just exits the whole damn app if it fails, so catch it here
		if (std::filesystem::path(modelPath).extension() != ".glb")
		{
			Craig::Logger::resources().warn("'{}' can't use {}, only .glb models work", mp_owner->getName(), modelPath);
			return CRAIG_FAIL;
		}

		Craig::ResourceManager::getInstance().loadModel(modelPath);
	}

	Craig::Logger::resources().debug("'{}' model: {}", mp_owner->getName(), modelPath.empty() ? "(none)" : modelPath);
	m_modelPath = modelPath;
	markSceneGeometryDirty();

	return ret;
}

void Craig::Components::Model::markSceneGeometryDirty()
{
	if (mp_owner != nullptr && mp_owner->getScene() != nullptr)
	{
		mp_owner->getScene()->markGeometryDirty();
	}
}

void Craig::Components::Model::displayImGuiAttributes()
{
	// edit a copy so we only try loading once the user's done typing
	std::string tempPath = m_modelPath;
	ImGui::InputText("Path", &tempPath);
	const bool pathEdited = ImGui::IsItemDeactivatedAfterEdit();

	ImGui::SameLine();
	if (ImGui::Button("Browse"))
	{
		m_modelBrowser.SetTitle("Select 3D Model");
		m_modelBrowser.SetDirectory("data/models");
		m_modelBrowser.SetTypeFilters({ ".glb" });
		m_modelBrowser.Open();
	}

	m_modelBrowser.Display();

	bool pathChanged = pathEdited;
	if (m_modelBrowser.HasSelected())
	{
		tempPath = std::filesystem::relative(m_modelBrowser.GetSelected(), std::filesystem::current_path()).string();
		m_modelBrowser.ClearSelected();
		pathChanged = true;
	}

	if (pathChanged)
	{
		switch (setModelPath(tempPath))
		{
		case CRAIG_SUCCESS:
			m_modelPathError.clear();
			break;
		case CRAIG_FILE_NOT_FOUND:
			m_modelPathError = "Couldn't find a file under that path";
			break;
		default:
			m_modelPathError = "Only .glb models are supported";
			break;
		}
	}

	if (!m_modelPathError.empty())
	{
		ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "%s", m_modelPathError.c_str());
	}
}
