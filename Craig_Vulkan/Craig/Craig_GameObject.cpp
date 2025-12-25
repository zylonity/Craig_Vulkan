#include "Craig_GameObject.hpp"
#include "Craig_ResourceManager.hpp"

CraigError Craig::GameObject::init() {

	CraigError ret = CRAIG_SUCCESS;
	Craig::ResourceManager::getInstance().loadModel(m_modelPath);
	return ret;
}

CraigError Craig::GameObject::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::GameObject::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


