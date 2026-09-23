#include "Craig_Scene.hpp"
#include "Craig_Utilities.hpp"
#include <filesystem>

CraigError Craig::Scene::init() {

	CraigError ret = CRAIG_SUCCESS;

	m_sun.lightDir = glm::vec3(0.5f, 1.0f, 0.25f);
	m_sun.lightColour = glm::vec3(1.0f, 0.98f, 0.95f);
	m_sun.ambientColour = glm::vec3(0.05f, 0.05f, 0.08f);

	// Build a 10 x 10 x 10 cube of evenly spaced ducks.
	const int cubeDimension = 10;
	const float duckScale = 0.01f;
	const float spacing = duckScale * 200.0f; // Gap between ducks.

	for (int x = 0; x < cubeDimension; x++)
	{
		for (int y = 0; y < cubeDimension; y++)
		{
			for (int z = 0; z < cubeDimension; z++)
			{
				Craig::GameObject* duck = new Craig::GameObject;

				const int index = x + (y * cubeDimension) + (z * cubeDimension * cubeDimension);

				duck->init("duck" + std::to_string(index), "data/models/Duck.glb", this);
				duck->setScale(glm::vec3(duckScale));
				duck->setPosition(glm::vec3(
					static_cast<float>(x) * spacing,
					static_cast<float>(y) * spacing,
					static_cast<float>(z) * spacing));
				mpv_Gameobjects.push_back(duck);
			}
		}
	}

	// Craig::GameObject* duck = new Craig::GameObject;
	//
	// duck->init("duck", "data/models/Duck.glb", this);
	// duck->setScale(glm::vec3(0.01f));
	//
	// mpv_Gameobjects.push_back(duck);


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


