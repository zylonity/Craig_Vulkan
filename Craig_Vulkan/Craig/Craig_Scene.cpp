#include "Craig_Scene.hpp"
#include "Craig_Utilities.hpp"
#include "../External/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

CraigError Craig::Scene::init(const std::string& scenePath) {

	CraigError ret = CRAIG_SUCCESS;

	std::ifstream sceneFile(scenePath);
	if (!sceneFile.is_open())
	{
		std::cerr << "Couldn't open scene file: " << scenePath << std::endl;
		return CRAIG_FILE_NOT_FOUND;
	}

	// Don't throw on bad json, just check if it got discarded
	const nlohmann::json sceneJson = nlohmann::json::parse(sceneFile, nullptr, false);
	if (sceneJson.is_discarded())
	{
		std::cerr << "Scene file isn't valid json: " << scenePath << std::endl;
		return CRAIG_FAIL;
	}

	m_scenePath = scenePath;
	m_name = sceneJson.value("name", std::filesystem::path(scenePath).stem().string());

	// sun, falls back to the old hardcoded values
	const nlohmann::json sunJson = sceneJson.value("sun", nlohmann::json::object());
	m_sun.lightDir = Utilities::readJsonVec3(sunJson, "direction", glm::vec3(0.5f, 1.0f, 0.25f));
	m_sun.lightColour = Utilities::readJsonVec3(sunJson, "colour", glm::vec3(1.0f, 0.98f, 0.95f));
	m_sun.ambientColour = Utilities::readJsonVec3(sunJson, "ambient", glm::vec3(0.05f, 0.05f, 0.08f));

	// camera start position + pitch/yaw
	const nlohmann::json cameraJson = sceneJson.value("camera", nlohmann::json::object());
	m_camera.setPosition(Utilities::readJsonVec3(cameraJson, "position", glm::vec3(0.0f)));
	m_camera.setPitchYaw(Utilities::readJsonVec2(cameraJson, "rotation", glm::vec2(0.0f)));

	// game objects
	for (const nlohmann::json& objectJson : sceneJson.value("gameObjects", nlohmann::json::array()))
	{
		const std::string objectName = objectJson.value("name", "");
		const std::string modelPath = objectJson.value("model", "");

		// skip anything broken instead of crashing the whole scene
		if (objectName.empty() || findObject(objectName) != nullptr || !std::filesystem::exists(modelPath))
		{
			std::cerr << "Skipping game object '" << objectName << "' in " << scenePath << " (no name, duplicate name or missing model)" << std::endl;
			continue;
		}

		Craig::GameObject* pObject = new Craig::GameObject;
		pObject->init(objectName, modelPath, this);
		pObject->setPosition(Utilities::readJsonVec3(objectJson, "position", glm::vec3(0.0f)));
		pObject->setRotation(Utilities::readJsonVec3(objectJson, "rotation", glm::vec3(0.0f))); // in degrees
		pObject->setScale(Utilities::readJsonVec3(objectJson, "scale", glm::vec3(1.0f)));
		mpv_Gameobjects.push_back(pObject);
	}

	// Sort the editor game object list by alphabetical order.
	Utilities::sortGameObjectsByName(mpv_Gameobjects);

	return ret;
}

Craig::GameObject* Craig::Scene::findObject(const std::string& objectName) const
{
	// Make sure a name has ben provided.
	assert(!objectName.empty());

	// Search game objects to see if one of them has the desired name.
	const std::vector<Craig::GameObject*>::const_iterator it = std::find_if(
		mpv_Gameobjects.begin(),
		mpv_Gameobjects.end(),
		[objectName](const Craig::GameObject* obj) { return obj->getName() == objectName; } // Comparison function
	);

	return it != mpv_Gameobjects.end() ? *it : nullptr;
}

void Craig::Scene::deleteGameObject(GameObject* const pObject)
{
	assert(pObject != nullptr);

	// Clean up the object's resouces
	pObject->terminate();

	// Remove the game object from the scene objects
	std::erase(mpv_Gameobjects, pObject);

	// Sort the editor game object list by alphabetical order.
	Utilities::sortGameObjectsByName(mpv_Gameobjects);

	// Free the memory allocated for the game object
	delete pObject;
}

CraigError Craig::Scene::newGameObject(std::string objectName, std::string modelPath, glm::vec3 position)
{

	CraigError ret = CRAIG_SUCCESS;

	// Check to see if name has been provided.
	if (objectName.empty())
	{
		return CRAIG_NO_NAME;
	}

	// Check to see if name is already in use.
	if (findObject(objectName) != nullptr)
	{
		return CRAIG_DUPLICATE_NAME;
	}

	if (std::filesystem::exists(modelPath) == false)
	{
		return CRAIG_FILE_NOT_FOUND;
	}

	// Create the game object in the scene.
	Craig::GameObject* tempObject = new Craig::GameObject;

	tempObject->init(objectName,modelPath, this);
	tempObject->setPosition(position);
	mpv_Gameobjects.push_back(tempObject);

	// Sort the editor game object list by alphabetical order.
	Utilities::sortGameObjectsByName(mpv_Gameobjects);

	return ret;
}

CraigError Craig::Scene::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;
	for (size_t i = 0; i < mpv_Gameobjects.size(); i++)
	{
		mpv_Gameobjects[i]->update();
	}
	return ret;
}


CraigError Craig::Scene::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	for (size_t i = 0; i < mpv_Gameobjects.size(); i++)
	{
		mpv_Gameobjects[i]->terminate();
		delete mpv_Gameobjects[i];
		mpv_Gameobjects[i] = nullptr;
	}
	mpv_Gameobjects.clear();
	return ret;
}


