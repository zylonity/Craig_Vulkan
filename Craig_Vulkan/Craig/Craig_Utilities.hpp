#pragma once
#include <string>
#include <vector>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../External/json.hpp"

#include "Craig/Craig_Constants.hpp"


namespace Craig {
	class GameObject;

	class Utilities {

	public:
		static bool displayVectorAttribute(
			const std::string& inputName,
			glm::vec3& attribute
		);

		static void sortGameObjectsByName(std::vector<GameObject*>& gameObjects);

		// Json vector helpers, reads return the fallback if the key is missing or the wrong size
		static glm::vec2 readJsonVec2(const nlohmann::json& json, const char* key, const glm::vec2& fallback);
		static glm::ivec2 readJsonVec2(const nlohmann::json& json, const char* key, const glm::ivec2& fallback);
		static glm::vec3 readJsonVec3(const nlohmann::json& json, const char* key, const glm::vec3& fallback);
		static glm::ivec3 readJsonVec3(const nlohmann::json& json, const char* key, const glm::ivec3& fallback);

		static void writeJsonVec2(nlohmann::json& json, const char* key, const glm::vec2& value);
		static void writeJsonVec2(nlohmann::json& json, const char* key, const glm::ivec2& value);
		static void writeJsonVec3(nlohmann::json& json, const char* key, const glm::vec3& value);
		static void writeJsonVec3(nlohmann::json& json, const char* key, const glm::ivec3& value);

		// Stored as [x, y, z, w] (same order as glTF), read gives back a normalised quat
		static glm::quat readJsonQuat(const nlohmann::json& json, const char* key, const glm::quat& fallback);
		static void writeJsonQuat(nlohmann::json& json, const char* key, const glm::quat& value);

	private:
		static bool compareStringsCaseInsensitive(std::string str1, std::string str2);

	};



}