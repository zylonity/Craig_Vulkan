#include "Craig_Scene.hpp"

CraigError Craig::Scene::init() {

	CraigError ret = CRAIG_SUCCESS;
	Craig::GameObject* m_MainObject = new Craig::GameObject;
	Craig::GameObject* m_secondObject= new Craig::GameObject;

	m_MainObject->init("phish","data/models/BarramundiFish.glb", this);
	m_MainObject->setPosition({m_MainObject->getPosition().x, m_MainObject->getPosition().y - 15, m_MainObject->getPosition().z});
	mpv_Gameobjects.push_back(m_MainObject);

	m_secondObject->init("fuck","data/models/Duck.glb", this);
	m_secondObject->setScale(glm::vec3(0.01f));
	mpv_Gameobjects.push_back(m_secondObject);
	//mv_Gameobjects.push_back(m_MainObject);
	// for (size_t i = 0; i < mv_Gameobjects.size(); i++)
	// {
	// 	mv_Gameobjects[i].init();
	// 	mv_Gameobjects[i].setPosition({mv_Gameobjects[i].getPosition().x + 10, mv_Gameobjects[i].getPosition().y, mv_Gameobjects[i].getPosition().z});
	// }
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


