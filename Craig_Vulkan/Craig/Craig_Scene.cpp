#include "Craig_Scene.hpp"

CraigError Craig::Scene::init() {

	CraigError ret = CRAIG_SUCCESS;

	mv_Gameobjects.push_back(m_MainObject);
	for (size_t i = 0; i < mv_Gameobjects.size(); i++)
	{
		mv_Gameobjects[i].init();
	}
	return ret;
}

CraigError Craig::Scene::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;
	for (size_t i = 0; i < mv_Gameobjects.size(); i++)
	{
		mv_Gameobjects[i].update();
	}
	return ret;
}


CraigError Craig::Scene::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	for (size_t i = 0; i < mv_Gameobjects.size(); i++)
	{
		mv_Gameobjects[i].terminate();
	}
	return ret;
}


