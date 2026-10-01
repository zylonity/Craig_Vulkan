#include "Craig_Sun.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <algorithm>

CraigError Craig::Components::Sun::init() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Components::Sun::update() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}


CraigError Craig::Components::Sun::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	return ret;
}

CraigError Craig::Components::Sun::loadFromJson(const nlohmann::json& json) {

	CraigError ret = CRAIG_SUCCESS;

	// Missing keys keep the defaults
	m_elevation = json.value("elevation", m_elevation);
	m_azimuth = json.value("azimuth", m_azimuth);
	mv3_lightColour = Utilities::readJsonVec3(json, "colour", mv3_lightColour);
	m_intensity = json.value("intensity", m_intensity);
	mv3_skyColour = Utilities::readJsonVec3(json, "skyColour", mv3_skyColour);
	mv3_groundColour = Utilities::readJsonVec3(json, "groundColour", mv3_groundColour);

	// old scenes saved a raw direction + one flat ambient, convert them so they still look the same
	if (!json.contains("elevation") && json.contains("direction"))
	{
		glm::vec3 dir = Utilities::readJsonVec3(json, "direction", getLightDir());
		if (glm::length(dir) > 0.0001f)
		{
			dir = glm::normalize(dir);
			m_elevation = glm::degrees(std::asin(std::clamp(dir.y, -1.0f, 1.0f)));
			m_azimuth = glm::degrees(std::atan2(dir.x, dir.z));
		}
	}
	if (!json.contains("skyColour") && json.contains("ambient"))
	{
		mv3_skyColour = Utilities::readJsonVec3(json, "ambient", mv3_skyColour);
		mv3_groundColour = mv3_skyColour;
	}

	return ret;
}

void Craig::Components::Sun::saveToJson(nlohmann::json& json) const {

	json["elevation"] = m_elevation;
	json["azimuth"] = m_azimuth;
	Utilities::writeJsonVec3(json, "colour", mv3_lightColour);
	json["intensity"] = m_intensity;
	Utilities::writeJsonVec3(json, "skyColour", mv3_skyColour);
	Utilities::writeJsonVec3(json, "groundColour", mv3_groundColour);
}

void Craig::Components::Sun::displayImGuiAttributes()
{
	// negative elevation puts the sun under the floor, handy for night
	ImGui::SliderFloat("Elevation", &m_elevation, -90.0f, 90.0f, "%.1f deg");
	ImGui::SliderFloat("Azimuth", &m_azimuth, -180.0f, 180.0f, "%.1f deg");

	ImGui::ColorEdit3("Colour", glm::value_ptr(mv3_lightColour));
	if (ImGui::SliderFloat("Temperature", &m_temperature, 1500.0f, 12000.0f, "%.0f K"))
	{
		mv3_lightColour = kelvinToColour(m_temperature);
	}
	ImGui::SetItemTooltip("Sets the colour, 2000K is a sunset and 6500K is midday");

	// anything over 1 just clips until there's HDR
	ImGui::DragFloat("Intensity", &m_intensity, 0.01f, 0.0f, 100.0f);

	ImGui::ColorEdit3("Sky Ambient", glm::value_ptr(mv3_skyColour));
	ImGui::ColorEdit3("Ground Ambient", glm::value_ptr(mv3_groundColour));
	ImGui::SetItemTooltip("Upward faces get the sky colour, downward faces get this, the rest blend between");
}

glm::vec3 Craig::Components::Sun::getLightDir() const {

	float elevation = glm::radians(m_elevation);
	float azimuth = glm::radians(m_azimuth);

	return glm::vec3(
		std::cos(elevation) * std::sin(azimuth),
		std::sin(elevation),
		std::cos(elevation) * std::cos(azimuth)
	);
}

// Tanner Helland's curve fit, good enough for picking colours
glm::vec3 Craig::Components::Sun::kelvinToColour(float kelvin) {

	float temp = kelvin / 100.0f;
	glm::vec3 colour;

	if (temp <= 66.0f)
	{
		colour.r = 255.0f;
		colour.g = 99.4708025861f * std::log(temp) - 161.1195681661f;
	}
	else
	{
		colour.r = 329.698727446f * std::pow(temp - 60.0f, -0.1332047592f);
		colour.g = 288.1221695283f * std::pow(temp - 60.0f, -0.0755148492f);
	}

	if (temp >= 66.0f)
		colour.b = 255.0f;
	else if (temp <= 19.0f)
		colour.b = 0.0f;
	else
		colour.b = 138.5177312231f * std::log(temp - 10.0f) - 305.0447927307f;

	colour = glm::clamp(colour / 255.0f, 0.0f, 1.0f);

	// the fit gives sRGB, lighting happens in linear so undo the gamma
	return glm::pow(colour, glm::vec3(2.2f));
}
