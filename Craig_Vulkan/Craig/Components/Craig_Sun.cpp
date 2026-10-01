#include "Craig_Sun.hpp"
#include "Craig/Craig_Utilities.hpp"
#include "imgui.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
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
	m_timeOfDay = json.value("timeOfDay", m_timeOfDay);
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

	json["timeOfDay"] = m_timeOfDay;
	json["elevation"] = m_elevation;
	json["azimuth"] = m_azimuth;
	Utilities::writeJsonVec3(json, "colour", mv3_lightColour);
	json["intensity"] = m_intensity;
	Utilities::writeJsonVec3(json, "skyColour", mv3_skyColour);
	Utilities::writeJsonVec3(json, "groundColour", mv3_groundColour);
}

void Craig::Components::Sun::displayImGuiAttributes()
{
	if (ImGui::SliderFloat("Time of Day", &m_timeOfDay, 0.0f, 24.0f, "%.1f h"))
	{
		applyTimeOfDay();
	}
	ImGui::SetItemTooltip("Sets everything below except the temperature slider, you can still tweak them after");

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

void Craig::Components::Sun::applyTimeOfDay() {

	// how high the sun gets at noon, 90 would be straight overhead
	const float maxElevation = 70.0f;

	// one full sine over 24h, peaks at noon and bottoms out at midnight
	float dayAngle = (m_timeOfDay - 6.0f) / 12.0f * glm::pi<float>();
	m_elevation = maxElevation * std::sin(dayAngle);

	// rises in the east (+x), sets in the west
	m_azimuth = 180.0f - m_timeOfDay * 15.0f;
	if (m_azimuth < -180.0f)
		m_azimuth += 360.0f;

	float height = std::sin(glm::radians(m_elevation));

	// fades out just after it dips under the horizon instead of snapping off
	m_intensity = glm::smoothstep(-0.05f, 0.25f, height);

	// orange near the horizon, whiter as it climbs
	m_temperature = glm::mix(2000.0f, 6500.0f, glm::smoothstep(0.0f, 0.6f, height));
	mv3_lightColour = kelvinToColour(m_temperature);

	// ambient goes from dark blue night to the normal day colours
	const glm::vec3 daySky = { 0.12f, 0.15f, 0.22f };
	const glm::vec3 dayGround = { 0.06f, 0.05f, 0.04f };
	const glm::vec3 nightSky = { 0.01f, 0.012f, 0.025f };
	const glm::vec3 nightGround = { 0.005f, 0.005f, 0.006f };
	const glm::vec3 duskTint = { 0.10f, 0.05f, 0.02f };

	float day = glm::smoothstep(-0.2f, 0.3f, height);
	// bit of warm glow in the sky around sunrise/sunset
	float dusk = 1.0f - glm::smoothstep(0.0f, 0.25f, std::abs(height));

	mv3_skyColour = glm::mix(nightSky, daySky, day) + duskTint * dusk;
	mv3_groundColour = glm::mix(nightGround, dayGround, day);
}
