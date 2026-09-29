#include "Craig_Utilities.hpp"

#include "Craig_GameObject.hpp"
#include "imgui.h"
#include <algorithm>

// Shared by all the json vector overloads, L = component count, T = float/int, Q = glm precision
template<glm::length_t L, typename T, glm::qualifier Q>
static glm::vec<L, T, Q> readJsonVec(const nlohmann::json& json, const char* key, const glm::vec<L, T, Q>& fallback)
{
	if (!json.contains(key) || !json[key].is_array() || json[key].size() != L)
	{
		return fallback;
	}

	glm::vec<L, T, Q> result;
	for (glm::length_t i = 0; i < L; i++)
	{
		// bail on anything that isn't a number, e.g. ["a", 1, 2]
		if (!json[key][i].is_number())
		{
			return fallback;
		}
		result[i] = json[key][i].get<T>();
	}
	return result;
}

template<glm::length_t L, typename T, glm::qualifier Q>
static void writeJsonVec(nlohmann::json& json, const char* key, const glm::vec<L, T, Q>& value)
{
	json[key] = nlohmann::json::array();
	for (glm::length_t i = 0; i < L; i++)
	{
		json[key].push_back(value[i]);
	}
}

//Vector3
bool Craig::Utilities::displayVectorAttribute(const std::string& inputName, glm::vec3& attribute)
{
	float vecToFloatArray[3] = { attribute.x, attribute.y, attribute.z };

	if(ImGui::InputFloat3(inputName.c_str(), vecToFloatArray))
	{
		attribute.x = vecToFloatArray[0];
		attribute.y = vecToFloatArray[1];
		attribute.z = vecToFloatArray[2];
		return true;
	}
	return false;
}


void Craig::Utilities::sortGameObjectsByName(std::vector<GameObject*>& gameObjects)
{
	// Sort using STL algorithm
	std::sort(gameObjects.begin(), gameObjects.end(),
		[](GameObject* a, GameObject* b) { return compareStringsCaseInsensitive(a->getName(), b->getName()); });
}

bool Craig::Utilities::compareStringsCaseInsensitive(std::string str1, std::string str2)
{
	// Convert each string to lowercase.
	for (char& str1Char : str1)
	{
		str1Char = std::tolower(str1Char);
	}
	for (char& str2Char : str2)
	{
		str2Char = std::tolower(str2Char);
	}

	// Use std::string internal comparison operator.
	return str1 < str2;
}

glm::vec2 Craig::Utilities::readJsonVec2(const nlohmann::json& json, const char* key, const glm::vec2& fallback) { return readJsonVec(json, key, fallback); }
glm::ivec2 Craig::Utilities::readJsonVec2(const nlohmann::json& json, const char* key, const glm::ivec2& fallback) { return readJsonVec(json, key, fallback); }
glm::vec3 Craig::Utilities::readJsonVec3(const nlohmann::json& json, const char* key, const glm::vec3& fallback) { return readJsonVec(json, key, fallback); }
glm::ivec3 Craig::Utilities::readJsonVec3(const nlohmann::json& json, const char* key, const glm::ivec3& fallback) { return readJsonVec(json, key, fallback); }

void Craig::Utilities::writeJsonVec2(nlohmann::json& json, const char* key, const glm::vec2& value) { writeJsonVec(json, key, value); }
void Craig::Utilities::writeJsonVec2(nlohmann::json& json, const char* key, const glm::ivec2& value) { writeJsonVec(json, key, value); }
void Craig::Utilities::writeJsonVec3(nlohmann::json& json, const char* key, const glm::vec3& value) { writeJsonVec(json, key, value); }
void Craig::Utilities::writeJsonVec3(nlohmann::json& json, const char* key, const glm::ivec3& value) { writeJsonVec(json, key, value); }

//Quaternion
// Components are read/written by name instead of index, glm's storage order changes with GLM_FORCE_QUAT_DATA_XYZW
glm::quat Craig::Utilities::readJsonQuat(const nlohmann::json& json, const char* key, const glm::quat& fallback)
{
	const glm::vec4 xyzw = readJsonVec(json, key, glm::vec4(fallback.x, fallback.y, fallback.z, fallback.w));

	// all zeros (or close) can't be normalised into a rotation
	if (glm::dot(xyzw, xyzw) < 0.000001f)
	{
		return fallback;
	}

	glm::quat result;
	result.x = xyzw.x;
	result.y = xyzw.y;
	result.z = xyzw.z;
	result.w = xyzw.w;
	return glm::normalize(result);
}

void Craig::Utilities::writeJsonQuat(nlohmann::json& json, const char* key, const glm::quat& value)
{
	writeJsonVec(json, key, glm::vec4(value.x, value.y, value.z, value.w));
}
