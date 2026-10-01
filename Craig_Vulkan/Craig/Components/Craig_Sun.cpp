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

	if (m_ambientFromSky)
	{
		updateAmbientFromSky();
	}

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
	m_ambientFromSky = json.value("ambientFromSky", m_ambientFromSky);
	m_skyAmbientStrength = json.value("skyAmbientStrength", m_skyAmbientStrength);
	mv3_groundAlbedo = Utilities::readJsonVec3(json, "groundAlbedo", mv3_groundAlbedo);

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
	json["ambientFromSky"] = m_ambientFromSky;
	json["skyAmbientStrength"] = m_skyAmbientStrength;
	Utilities::writeJsonVec3(json, "groundAlbedo", mv3_groundAlbedo);
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

	// highlights clip to white without HDR, that's expected
	ImGui::DragFloat("Intensity", &m_intensity, 0.01f, 0.0f, 100.0f);

	ImGui::Checkbox("Ambient From Sky", &m_ambientFromSky);
	ImGui::SetItemTooltip("Works the ambient out from the sky so models match the background");
	if (m_ambientFromSky)
	{
		ImGui::DragFloat("Sky Ambient Strength", &m_skyAmbientStrength, 0.01f, 0.0f, 10.0f);
		ImGui::ColorEdit3("Ground Albedo", glm::value_ptr(mv3_groundAlbedo));
		ImGui::SetItemTooltip("How much light the floor bounces back up onto downward faces");
	}

	// these just show what the sky worked out when it's on
	ImGui::BeginDisabled(m_ambientFromSky);
	ImGui::ColorEdit3("Sky Ambient", glm::value_ptr(mv3_skyColour));
	ImGui::ColorEdit3("Ground Ambient", glm::value_ptr(mv3_groundColour));
	ImGui::SetItemTooltip("Upward faces get the sky colour, downward faces get this, the rest blend between");
	ImGui::EndDisabled();
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
	const float noonIntensity = 3.0f;

	// one full sine over 24h, peaks at noon and bottoms out at midnight
	float dayAngle = (m_timeOfDay - 6.0f) / 12.0f * glm::pi<float>();
	m_elevation = maxElevation * std::sin(dayAngle);

	// rises in the east (+x), sets in the west
	m_azimuth = 180.0f - m_timeOfDay * 15.0f;
	if (m_azimuth < -180.0f)
		m_azimuth += 360.0f;

	float height = std::sin(glm::radians(m_elevation));

	// fades out just after it dips under the horizon instead of snapping off
	m_intensity = noonIntensity * glm::smoothstep(-0.05f, 0.25f, height);

	// orange near the horizon, whiter as it climbs
	m_temperature = glm::mix(2000.0f, 6500.0f, glm::smoothstep(0.0f, 0.6f, height));
	mv3_lightColour = kelvinToColour(m_temperature);

	// ambient goes from dark blue night to the normal day colours
	// (gets overwritten next frame if Ambient From Sky is on)
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

// rsi() and atmosphere() are a straight C++ port of glsl-atmosphere by Rye Terrell (wwwtyro), Unlicense (public domain)
// https://github.com/wwwtyro/glsl-atmosphere/blob/master/index.glsl
// same maths as SkyFragmentShader.frag, keep them in sync if you change one
namespace {

	constexpr int kAtmosphereISteps = 16;
	constexpr int kAtmosphereJSteps = 8;

	glm::vec2 rsi(glm::vec3 r0, glm::vec3 rd, float sr) {
		// ray-sphere intersection that assumes
		// the sphere is centered at the origin.
		// No intersection when result.x > result.y
		float a = glm::dot(rd, rd);
		float b = 2.0f * glm::dot(rd, r0);
		float c = glm::dot(r0, r0) - (sr * sr);
		float d = (b * b) - 4.0f * a * c;
		if (d < 0.0f) return glm::vec2(1e5f, -1e5f);
		return glm::vec2(
			(-b - std::sqrt(d)) / (2.0f * a),
			(-b + std::sqrt(d)) / (2.0f * a)
		);
	}

	glm::vec3 atmosphere(glm::vec3 r, glm::vec3 r0, glm::vec3 pSun, float iSun, float rPlanet, float rAtmos, glm::vec3 kRlh, float kMie, float shRlh, float shMie, float g) {
		// Normalize the sun and view directions.
		pSun = glm::normalize(pSun);
		r = glm::normalize(r);

		// Calculate the step size of the primary ray.
		glm::vec2 p = rsi(r0, r, rAtmos);
		if (p.x > p.y) return glm::vec3(0.0f);
		p.y = std::min(p.y, rsi(r0, r, rPlanet).x);
		float iStepSize = (p.y - p.x) / float(kAtmosphereISteps);

		// Initialize the primary ray time.
		float iTime = 0.0f;

		// Initialize accumulators for Rayleigh and Mie scattering.
		glm::vec3 totalRlh = glm::vec3(0.0f);
		glm::vec3 totalMie = glm::vec3(0.0f);

		// Initialize optical depth accumulators for the primary ray.
		float iOdRlh = 0.0f;
		float iOdMie = 0.0f;

		// Calculate the Rayleigh and Mie phases.
		float mu = glm::dot(r, pSun);
		float mumu = mu * mu;
		float gg = g * g;
		float pRlh = 3.0f / (16.0f * glm::pi<float>()) * (1.0f + mumu);
		float pMie = 3.0f / (8.0f * glm::pi<float>()) * ((1.0f - gg) * (mumu + 1.0f)) / (std::pow(1.0f + gg - 2.0f * mu * g, 1.5f) * (2.0f + gg));

		// Sample the primary ray.
		for (int i = 0; i < kAtmosphereISteps; i++) {

			// Calculate the primary ray sample position.
			glm::vec3 iPos = r0 + r * (iTime + iStepSize * 0.5f);

			// Calculate the height of the sample.
			float iHeight = glm::length(iPos) - rPlanet;

			// Calculate the optical depth of the Rayleigh and Mie scattering for this step.
			float odStepRlh = std::exp(-iHeight / shRlh) * iStepSize;
			float odStepMie = std::exp(-iHeight / shMie) * iStepSize;

			// Accumulate optical depth.
			iOdRlh += odStepRlh;
			iOdMie += odStepMie;

			// Calculate the step size of the secondary ray.
			float jStepSize = rsi(iPos, pSun, rAtmos).y / float(kAtmosphereJSteps);

			// Initialize the secondary ray time.
			float jTime = 0.0f;

			// Initialize optical depth accumulators for the secondary ray.
			float jOdRlh = 0.0f;
			float jOdMie = 0.0f;

			// Sample the secondary ray.
			for (int j = 0; j < kAtmosphereJSteps; j++) {

				// Calculate the secondary ray sample position.
				glm::vec3 jPos = iPos + pSun * (jTime + jStepSize * 0.5f);

				// Calculate the height of the sample.
				float jHeight = glm::length(jPos) - rPlanet;

				// Accumulate the optical depth.
				jOdRlh += std::exp(-jHeight / shRlh) * jStepSize;
				jOdMie += std::exp(-jHeight / shMie) * jStepSize;

				// Increment the secondary ray time.
				jTime += jStepSize;
			}

			// Calculate attenuation.
			glm::vec3 attn = glm::exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));

			// Accumulate scattering.
			totalRlh += odStepRlh * attn;
			totalMie += odStepMie * attn;

			// Increment the primary ray time.
			iTime += iStepSize;

		}

		// Calculate and return the final color.
		return iSun * (pRlh * kRlh * totalRlh + pMie * kMie * totalMie);
	}

	// same settings + exposure as the sky shader, so this is the colour you actually see on screen
	glm::vec3 skyColourInDirection(const glm::vec3& dir, const glm::vec3& sunDir) {
		glm::vec3 colour = atmosphere(
			dir,
			glm::vec3(0.0f, 6372e3f, 0.0f),
			sunDir,
			22.0f,
			6371e3f,
			6471e3f,
			glm::vec3(5.5e-6f, 13.0e-6f, 22.4e-6f),
			21e-6f,
			8e3f,
			1.2e3f,
			0.758f
		);
		return glm::vec3(1.0f) - glm::exp(-colour);
	}
}

void Craig::Components::Sun::updateAmbientFromSky() {

	glm::vec3 sunDir = glm::normalize(getLightDir());

	// Average the sky over the top half of the world, weighted by how much each bit faces straight up
	// 8 around x 4 up = 32 atmosphere calls, fine once in a while but don't do it every frame
	if (glm::length(sunDir - mv3_skyAverageSunDir) > 0.0001f)
	{
		const float elevations[] = { 90.0f, 60.0f, 30.0f, 10.0f };
		glm::vec3 total = glm::vec3(0.0f);
		float totalWeight = 0.0f;

		for (int around = 0; around < 8; around++)
		{
			float azimuth = glm::radians(around * 45.0f);
			for (float elevationDeg : elevations)
			{
				float elevation = glm::radians(elevationDeg);
				glm::vec3 dir = {
					std::cos(elevation) * std::sin(azimuth),
					std::sin(elevation),
					std::cos(elevation) * std::cos(azimuth)
				};

				float weight = std::sin(elevation);
				total += skyColourInDirection(dir, sunDir) * weight;
				totalWeight += weight;
			}
		}

		mv3_skyAverage = total / totalWeight;
		mv3_skyAverageSunDir = sunDir;
	}

	// tiny floor so night isn't pitch black
	const glm::vec3 nightAmbient = { 0.01f, 0.012f, 0.025f };
	mv3_skyColour = glm::max(mv3_skyAverage * m_skyAmbientStrength, nightAmbient);

	// downward faces see the floor, which is lit by the sun + the sky and bounces some of it back
	// sun part matches the shader's diffuse (albedo / pi * light * NdotL), cheap enough to do every frame
	glm::vec3 sunOnGround = mv3_lightColour * m_intensity * std::max(sunDir.y, 0.0f) / glm::pi<float>();
	mv3_groundColour = mv3_groundAlbedo * (sunOnGround + mv3_skyColour);
}
