#pragma once
#include <string>
#include <vector>
#include <glm/vec3.hpp>

#include "Craig/Craig_Constants.hpp"


namespace Craig {
	class GameObject;

	class Utilities {

	public:
		static void displayVectorAttribute(
			const std::string& inputName,
			glm::vec3& attribute
		);

		static void sortGameObjectsByName(std::vector<GameObject*>& gameObjects);

	private:
		static bool compareStringsCaseInsensitive(std::string str1, std::string str2);

	};



}