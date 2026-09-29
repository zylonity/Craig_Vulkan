#include "Craig_RigidBody.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>

CraigError Craig::Components::RigidBody::init() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Components::RigidBody::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::Components::RigidBody::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Components::RigidBody::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Missing keys keep the defaults
	mv3_lightDir = Utilities::readJsonVec3(json, "direction", mv3_lightDir);
	mv3_lightColour = Utilities::readJsonVec3(json, "colour", mv3_lightColour);
	mv3_ambientColour = Utilities::readJsonVec3(json, "ambient", mv3_ambientColour);

	return ret;
}

void Craig::Components::RigidBody::saveToJson(nlohmann::json& json) const {

	Utilities::writeJsonVec3(json, "direction", mv3_lightDir);
	Utilities::writeJsonVec3(json, "colour", mv3_lightColour);
	Utilities::writeJsonVec3(json, "ambient", mv3_ambientColour);
}

void Craig::Components::RigidBody::displayImGuiAttributes()
{
	// Direction doesn't need to be normalised, the shader does that
	ImGui::DragFloat3("Direction", glm::value_ptr(mv3_lightDir), 0.01f);
	ImGui::ColorEdit3("Colour", glm::value_ptr(mv3_lightColour));
	ImGui::ColorEdit3("Ambient", glm::value_ptr(mv3_ambientColour));
}
