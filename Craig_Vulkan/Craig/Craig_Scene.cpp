#include "Craig_Scene.hpp"
#include "Craig_Utilities.hpp"
#include "Craig_Logger.hpp"
#include "Components/Craig_Model.hpp"
#include "Components/Craig_Sun.hpp"
#include "Components/Craig_BoxCollider.hpp"
#include "Components/Craig_SphereCollider.hpp"
#include "Components/Craig_CapsuleCollider.hpp"
#include "Components/Craig_ConvexCollider.hpp"
#include "Components/Craig_RigidBody.hpp"
#include "../External/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>

CraigError Craig::Scene::init(const std::string& scenePath, Craig::PhysicsEngine* pPhysicsEngine) {

	Craig::Logger::scene().info("Loading scene {}", scenePath);

	std::ifstream sceneFile(scenePath);
	if (!sceneFile.is_open())
	{
		Craig::Logger::scene().error("Couldn't open scene file: {}", scenePath);
		return CRAIG_FILE_NOT_FOUND;
	}

	// Don't throw on bad json, just check if it got discarded
	const nlohmann::json sceneJson = nlohmann::json::parse(sceneFile, nullptr, false);
	if (sceneJson.is_discarded())
	{
		Craig::Logger::scene().error("Scene file isn't valid json: {}", scenePath);
		return CRAIG_FAIL;
	}

	return initFromJson(sceneJson, scenePath, pPhysicsEngine);
}

CraigError Craig::Scene::initFromJson(const nlohmann::json& sceneJson, const std::string& scenePath, Craig::PhysicsEngine* pPhysicsEngine) {

	CraigError ret = CRAIG_SUCCESS;

	const spdlog::stopwatch loadTimer;

	// set before anything loads so components can reach it
	mp_physicsEngine = pPhysicsEngine;

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
			Craig::Logger::scene().warn("Skipping game object '{}' in {} (no name or duplicate name)", objectName, scenePath);
			continue;
		}

		Craig::GameObject* pObject = new Craig::GameObject;
		pObject->init(objectName, this);
		pObject->setPosition(Utilities::readJsonVec3(objectJson, "position", glm::vec3(0.0f)));
		pObject->setRotation(Utilities::readJsonVec3(objectJson, "rotation", glm::vec3(0.0f))); // in degrees
		pObject->setScale(Utilities::readJsonVec3(objectJson, "scale", glm::vec3(1.0f)));
		mpv_Gameobjects.push_back(pObject);

		loadComponentsFromJson(pObject, objectJson.value("components", nlohmann::json::object()));
		Craig::Logger::scene().debug("Loaded '{}' with {} component(s)", objectName, pObject->getComponents().size());
	}

	// Sort the editor game object list by alphabetical order.
	Utilities::sortGameObjectsByName(mpv_Gameobjects);

	// whoever loads the scene builds the buffers for it, so nothing's dirty yet
	m_geometryDirty = false;

	Craig::Logger::scene().info("Loaded scene '{}': {} game objects in {:.1f} ms", m_name, mpv_Gameobjects.size(), loadTimer.elapsed().count() * 1000.0);
	if (getSun() == nullptr) {
		Craig::Logger::scene().warn("'{}' has no sun, it's going to be pretty dark", m_name);
	}

	return ret;
}

nlohmann::json Craig::Scene::toJson() const {

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

			// types that can have more then one (colliders) get an array under their key
			if (pComponent->allowMultiple())
			{
				objectJson["components"][pComponent->getJsonKey()].push_back(componentJson);
			}
			else
			{
				objectJson["components"][pComponent->getJsonKey()] = componentJson;
			}
		}

		sceneJson["gameObjects"].push_back(objectJson);
	}

	return sceneJson;
}

CraigError Craig::Scene::save() {

	CraigError ret = CRAIG_SUCCESS;

	const nlohmann::json sceneJson = toJson();

	// Write to a temp file first and swap it in, so a failed write can't wipe the old scene
	const std::filesystem::path scenePath = m_scenePath;
	std::filesystem::path tempPath = scenePath;
	tempPath += ".tmp";

	std::ofstream sceneFile(tempPath);
	if (!sceneFile.is_open())
	{
		Craig::Logger::scene().error("Couldn't open {} for writing", tempPath.string());
		return CRAIG_FAIL;
	}
	sceneFile << sceneJson.dump(2) << std::endl;
	sceneFile.close();
	if (sceneFile.fail())
	{
		Craig::Logger::scene().error("Couldn't write {}", tempPath.string());
		return CRAIG_FAIL;
	}

	std::error_code error;
	std::filesystem::rename(tempPath, scenePath, error);
	if (error)
	{
		Craig::Logger::scene().error("Couldn't replace {}: {}", scenePath.string(), error.message());
		return CRAIG_FAIL;
	}

	Craig::Logger::scene().info("Saved scene '{}' to {} ({} game objects)", m_name, scenePath.string(), mpv_Gameobjects.size());

	return ret;
}

// types that can have more than one get saved as an array under their key
// a single object still loads too, older scenes were saved like that
template<typename T>
static void loadMultipleComponentsFromJson(Craig::GameObject* pObject, const nlohmann::json& componentsJson, const char* key)
{
	if (!componentsJson.contains(key))
	{
		return;
	}

	const nlohmann::json& json = componentsJson[key];
	if (json.is_array())
	{
		for (const nlohmann::json& componentJson : json)
		{
			pObject->addComponent<T>()->loadFromJson(componentJson);
		}
	}
	else
	{
		pObject->addComponent<T>()->loadFromJson(json);
	}
}

// Components are keyed by type, e.g. "components": { "model": { "path": "..." }, "sun": { ... }, "boxCollider": [ { ... }, { ... } ] }
void Craig::Scene::loadComponentsFromJson(Craig::GameObject* pObject, const nlohmann::json& componentsJson)
{
	// typos in the json used to just get ignored, now you'll hear about it
	constexpr const char* kKnownComponentKeys[] = { "model", "sun", "boxCollider", "sphereCollider", "capsuleCollider", "convexCollider", "rigidBody" };
	for (const auto& [key, value] : componentsJson.items())
	{
		if (std::find(std::begin(kKnownComponentKeys), std::end(kKnownComponentKeys), key) == std::end(kKnownComponentKeys))
		{
			Craig::Logger::scene().warn("Unknown component '{}' on '{}' in {}, ignoring it", key, pObject->getName(), m_scenePath);
		}
	}

	if (componentsJson.contains("model"))
	{
		Components::Model* pModel = pObject->addComponent<Components::Model>();
		if (pModel->loadFromJson(componentsJson["model"]) != CRAIG_SUCCESS)
		{
			Craig::Logger::scene().warn("Couldn't load the model for '{}' in {} (missing file or not a .glb)", pObject->getName(), m_scenePath);
			pObject->removeComponent(pModel);
		}
	}

	if (componentsJson.contains("sun"))
	{
		// only one sun per scene, first one wins
		if (getSun() != nullptr)
		{
			Craig::Logger::scene().warn("Skipping sun on '{}' in {}, the scene already has one", pObject->getName(), m_scenePath);
		}
		else
		{
			pObject->addComponent<Components::Sun>()->loadFromJson(componentsJson["sun"]);
		}
	}

	loadMultipleComponentsFromJson<Components::BoxCollider>(pObject, componentsJson, "boxCollider");
	loadMultipleComponentsFromJson<Components::SphereCollider>(pObject, componentsJson, "sphereCollider");
	loadMultipleComponentsFromJson<Components::CapsuleCollider>(pObject, componentsJson, "capsuleCollider");
	loadMultipleComponentsFromJson<Components::ConvexCollider>(pObject, componentsJson, "convexCollider");

	if (componentsJson.contains("rigidBody"))
	{
		pObject->addComponent<Components::RigidBody>()->loadFromJson(componentsJson["rigidBody"]);
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

	Craig::Logger::scene().info("Deleted game object '{}'", pObject->getName());

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
		Craig::Logger::scene().warn("Can't make a game object with no name");
		return CRAIG_NO_NAME;
	}

	// Check to see if name is already in use.
	if (findObject(objectName) != nullptr)
	{
		Craig::Logger::scene().warn("There's already a game object called '{}'", objectName);
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
			Craig::Logger::scene().error("Couldn't give '{}' the model {}, not making it", objectName, modelPath);
			tempObject->terminate();
			delete tempObject;
			return ret;
		}
	}

	tempObject->setPosition(position);
	mpv_Gameobjects.push_back(tempObject);

	Craig::Logger::scene().info("New game object '{}'{} at ({:.2f}, {:.2f}, {:.2f})", objectName, modelPath.empty() ? "" : " with " + modelPath, position.x, position.y, position.z);

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

CraigError Craig::Scene::gameplayUpdate(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;
	// index loop on purpose, gameplay might spawn stuff and push_back would break a range-for
	for (size_t i = 0; i < mpv_Gameobjects.size(); i++)
	{
		mpv_Gameobjects[i]->gameplayUpdate(deltaTime);
	}
	return ret;
}


CraigError Craig::Scene::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	Craig::Logger::scene().debug("Unloading scene '{}' ({} game objects)", m_name, mpv_Gameobjects.size());

	for (size_t i = 0; i < mpv_Gameobjects.size(); i++)
	{
		mpv_Gameobjects[i]->terminate();
		delete mpv_Gameobjects[i];
		mpv_Gameobjects[i] = nullptr;
	}
	mpv_Gameobjects.clear();
	return ret;
}


