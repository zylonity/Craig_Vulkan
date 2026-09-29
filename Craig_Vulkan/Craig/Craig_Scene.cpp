#include "Craig_Scene.hpp"
#include "Craig_Utilities.hpp"
#include "Components/Craig_Model.hpp"
#include "Components/Craig_Sun.hpp"
#include "Components/Craig_BoxCollider.hpp"
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

	// camera start position + pitch/yaw
	const nlohmann::json cameraJson = sceneJson.value("camera", nlohmann::json::object());
	m_camera.setPosition(Utilities::readJsonVec3(cameraJson, "position", glm::vec3(0.0f)));
	m_camera.setPitchYaw(Utilities::readJsonVec2(cameraJson, "rotation", glm::vec2(0.0f)));

	// game objects
	for (const nlohmann::json& objectJson : sceneJson.value("gameObjects", nlohmann::json::array()))
	{
		const std::string objectName = objectJson.value("name", "");

		// skip anything broken instead of crashing the whole scene
		if (objectName.empty() || findObject(objectName) != nullptr)
		{
			std::cerr << "Skipping game object '" << objectName << "' in " << scenePath << " (no name or duplicate name)" << std::endl;
			continue;
		}

		Craig::GameObject* pObject = new Craig::GameObject;
		pObject->init(objectName, this);
		pObject->setPosition(Utilities::readJsonVec3(objectJson, "position", glm::vec3(0.0f)));
		pObject->setRotation(Utilities::readJsonVec3(objectJson, "rotation", glm::vec3(0.0f))); // in degrees
		pObject->setScale(Utilities::readJsonVec3(objectJson, "scale", glm::vec3(1.0f)));
		mpv_Gameobjects.push_back(pObject);

		loadComponentsFromJson(pObject, objectJson.value("components", nlohmann::json::object()));
	}

	// Sort the editor game object list by alphabetical order.
	Utilities::sortGameObjectsByName(mpv_Gameobjects);

	// whoever loads the scene builds the buffers for it, so nothing's dirty yet
	m_geometryDirty = false;

	return ret;
}

CraigError Craig::Scene::save() {

	CraigError ret = CRAIG_SUCCESS;

	// same layout init() reads
	nlohmann::json sceneJson;
	sceneJson["name"] = m_name;

	nlohmann::json cameraJson = nlohmann::json::object();
	Utilities::writeJsonVec3(cameraJson, "position", m_camera.getPosition());
	Utilities::writeJsonVec2(cameraJson, "rotation", m_camera.getRotation());
	sceneJson["camera"] = cameraJson;

	sceneJson["gameObjects"] = nlohmann::json::array();
	for (const Craig::GameObject* pObject : mpv_Gameobjects)
	{
		nlohmann::json objectJson;
		objectJson["name"] = pObject->getName();
		Utilities::writeJsonVec3(objectJson, "position", pObject->getPosition());
		Utilities::writeJsonVec3(objectJson, "rotation", pObject->getRotation()); // in degrees
		Utilities::writeJsonVec3(objectJson, "scale", pObject->getScale());

		objectJson["components"] = nlohmann::json::object();
		for (const std::unique_ptr<Components::Component>& pComponent : pObject->getComponents())
		{
			nlohmann::json componentJson = nlohmann::json::object();
			pComponent->saveToJson(componentJson);
			objectJson["components"][pComponent->getJsonKey()] = componentJson;
		}

		sceneJson["gameObjects"].push_back(objectJson);
	}

	// Write to a temp file first and swap it in, so a failed write can't wipe the old scene
	const std::filesystem::path scenePath = m_scenePath;
	std::filesystem::path tempPath = scenePath;
	tempPath += ".tmp";

	std::ofstream sceneFile(tempPath);
	if (!sceneFile.is_open())
	{
		std::cerr << "Couldn't open " << tempPath << " for writing" << std::endl;
		return CRAIG_FAIL;
	}
	sceneFile << sceneJson.dump(2) << std::endl;
	sceneFile.close();
	if (sceneFile.fail())
	{
		std::cerr << "Couldn't write " << tempPath << std::endl;
		return CRAIG_FAIL;
	}

	std::error_code error;
	std::filesystem::rename(tempPath, scenePath, error);
	if (error)
	{
		std::cerr << "Couldn't replace " << scenePath << ": " << error.message() << std::endl;
		return CRAIG_FAIL;
	}

	return ret;
}

// Components are keyed by type, e.g. "components": { "model": { "path": "..." }, "sun": { ... } }
void Craig::Scene::loadComponentsFromJson(Craig::GameObject* pObject, const nlohmann::json& componentsJson)
{
	if (componentsJson.contains("model"))
	{
		Components::Model* pModel = pObject->addComponent<Components::Model>();
		if (pModel->loadFromJson(componentsJson["model"]) != CRAIG_SUCCESS)
		{
			std::cerr << "Couldn't load the model for '" << pObject->getName() << "' in " << m_scenePath << " (missing file or not a .glb)" << std::endl;
			pObject->removeComponent(pModel);
		}
	}

	if (componentsJson.contains("sun"))
	{
		// only one sun per scene, first one wins
		if (getSun() != nullptr)
		{
			std::cerr << "Skipping sun on '" << pObject->getName() << "' in " << m_scenePath << ", the scene already has one" << std::endl;
		}
		else
		{
			pObject->addComponent<Components::Sun>()->loadFromJson(componentsJson["sun"]);
		}
	}

	if (componentsJson.contains("boxCollider"))
	{
		pObject->addComponent<Components::BoxCollider>()->loadFromJson(componentsJson["boxCollider"]);
	}
}

Craig::Components::Sun* Craig::Scene::getSun() const
{
	for (const Craig::GameObject* pObject : mpv_Gameobjects)
	{
		if (Components::Sun* pSun = pObject->getComponent<Components::Sun>())
		{
			return pSun;
		}
	}
	return nullptr;
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

	// Create the game object in the scene.
	Craig::GameObject* tempObject = new Craig::GameObject;
	tempObject->init(objectName, this);

	// an empty path makes an empty game object, otherwise it gets a model component
	if (!modelPath.empty())
	{
		ret = tempObject->addComponent<Components::Model>()->setModelPath(modelPath);
		if (ret != CRAIG_SUCCESS)
		{
			tempObject->terminate();
			delete tempObject;
			return ret;
		}
	}

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


