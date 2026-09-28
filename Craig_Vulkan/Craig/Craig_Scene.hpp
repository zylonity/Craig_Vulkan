#pragma once
#include "Craig_Constants.hpp"
#include "Craig_GameObject.hpp"

#include <vector>

#include "Craig_Camera.hpp"
#include "../External/json.hpp"

namespace Craig {
	namespace Components { class Sun; }

	class Scene {

	public:
		CraigError init(const std::string& scenePath);
		CraigError update(const float& deltaTime);
		CraigError terminate();

		// writes the scene back to the .json it was loaded from
		CraigError save();

		const std::string& getName() const { return m_name; }
		const std::string& getScenePath() const { return m_scenePath; }

		std::vector<Craig::GameObject*>& getGameObjects() { return mpv_Gameobjects; }
		Craig::GameObject* findObject(const std::string& objectName) const;

		Craig::Camera& getCamera() { return m_camera; }
		// the scene's sun component, nullptr if no game object has one
		Components::Sun* getSun() const;
		void deleteGameObject(Craig::GameObject* gameObject);
		CraigError newGameObject(std::string objectName, std::string modelPath, glm::vec3 position);

		// Set when a model gets added/changed/removed, the renderer rebuilds its vertex/index buffers when it sees it
		void markGeometryDirty() { m_geometryDirty = true; }
		bool consumeGeometryDirty() { const bool wasDirty = m_geometryDirty; m_geometryDirty = false; return wasDirty; }
	private:
		void loadComponentsFromJson(Craig::GameObject* pObject, const nlohmann::json& componentsJson);

		std::string m_name;
		std::string m_scenePath; // The .json file this scene was loaded from

		std::vector<Craig::GameObject*> mpv_Gameobjects;

		Craig::Camera m_camera = Craig::Camera(); //Virtual camera for the scene
		bool m_geometryDirty = false;
	};



}
