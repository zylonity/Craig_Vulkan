#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Craig_Constants.hpp"


namespace Craig {

	class GameObject {

	public:
		CraigError init(std::string modelPath);
		CraigError update();
		CraigError terminate();

		glm::mat4 GetModelMatrix() { return m_modelMatrix; }

		void setPosition(glm::vec3 position);
		void setScale(glm::vec3 scale);
		const glm::vec3& getPosition() const { return mv3_position; }

		const std::string& getModelPath() const { return m_modelPath; }

		std::vector<vk::DescriptorSet>& getDescriptorSets() { return mv_VK_descriptorSets; }
	private:
		std::vector<vk::DescriptorSet> mv_VK_descriptorSets = std::vector<vk::DescriptorSet>(kMaxFramesInFlight);

		void updateModelMatrix();

		glm::vec3 mv3_position{};
		glm::vec3 mv3_rotation{};
		glm::vec3 mv3_scale = glm::vec3(1);

		glm::mat4 m_modelMatrix = glm::mat4(1);
		glm::mat4 m_inverseModelMatrix{};

		std::string m_modelPath;// = "data/models/BarramundiFish.glb";



	};



}