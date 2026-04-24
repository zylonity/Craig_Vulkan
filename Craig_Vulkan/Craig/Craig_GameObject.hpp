#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Craig_Constants.hpp"


namespace Craig {
	class Scene;

	class GameObject {

	public:
		CraigError init(std::string name, std::string modelPath, Craig::Scene* scenePtr);
		CraigError update();
		CraigError terminate();

		glm::mat4 GetModelMatrix() { return m_modelMatrix; }

		const glm::vec3& getPosition() const { return mv3_position; }
		const glm::vec3& getRotation() const { return mv3_rotation; }
		const glm::vec3& getScale() const { return mv3_scale; }
		const glm::quat& getRotationQuat() const { return m_rotationQuat; }

		void setPosition(glm::vec3 position) { mv3_position = position; };
		void setRotation(glm::vec3 rotation);
		void setScale(glm::vec3 scale)		 { mv3_scale = scale; };
		void setRotationQuat(const glm::quat& q);

		const std::string& getModelPath() const { return m_modelPath; }
		const std::string& getName() const { return m_name; }

		void displayImGuiAttributes();
	private:
		void updateModelMatrix();

		glm::vec3 mv3_position{};
		glm::vec3 mv3_rotation{};
		glm::vec3 mv3_scale = glm::vec3(1);
		glm::quat m_rotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		glm::mat4 m_modelMatrix = glm::mat4(1);
		glm::mat4 m_inverseModelMatrix{};

		std::string m_modelPath;
		std::string m_name;

		Craig::Scene* mp_scene;

		//TODO: Maybe some sort of UUID system? Currently relying on either the name or the pointer, either of which could easily mess up


	};



}