#include "Craig_Utilities.hpp"

#include "Craig_GameObject.hpp"
#include "imgui.h"
#include <algorithm>

//Vector3
void Craig::Utilities::displayVectorAttribute(const std::string& inputName, glm::vec3& attribute)
{
	float vecToFloatArray[3] = { attribute.x, attribute.y, attribute.z };

	if(ImGui::InputFloat3(inputName.c_str(), vecToFloatArray))
	{
		attribute.x = vecToFloatArray[0];
		attribute.y = vecToFloatArray[1];
		attribute.z = vecToFloatArray[2];
	}
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
