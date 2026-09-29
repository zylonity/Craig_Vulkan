#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Craig_Constants.hpp"
#include "Components/Craig_Component.hpp"


namespace Craig {
	class Scene;

	class GameObject {

	public:
		CraigError init(std::string name, Craig::Scene* scenePtr);
		CraigError update();
		CraigError terminate();

		glm::mat4 GetModelMatrix() { return m_modelMatrix; }
		// Built from the current pos/rot/scale, GetModelMatrix() is only refreshed in update() so can be a frame behind
		glm::mat4 calculateModelMatrix() const;

		const glm::vec3& getPosition() const { return mv3_position; }
		const glm::vec3& getRotation() const { return mv3_rotation; }
		const glm::vec3& getScale() const { return mv3_scale; }
		const glm::quat& getRotationQuat() const { return m_rotationQuat; }

		void setPosition(glm::vec3 position) { mv3_position = position; };
		void setRotation(glm::vec3 rotation);
		void setScale(glm::vec3 scale)		 { mv3_scale = scale; };
		void setRotationQuat(const glm::quat& q);

		const std::string& getName() const { return m_name; }
		Craig::Scene* getScene() const { return mp_scene; }

		// Returns the component of type T, or nullptr if this object doesn't have one
		template<typename T>
		T* getComponent() const
		{
			for (const std::unique_ptr<Components::Component>& pComponent : mv_components)
			{
				if (T* pFound = dynamic_cast<T*>(pComponent.get()))
				{
					return pFound;
				}
			}
			return nullptr;
		}

		// Only one of each component type per object, returns nullptr if it already has one
		// Every component of that type (or derived from it, e.g. getComponents<Collider>() gets all the collider shapes)
		template<typename T>
		std::vector<T*> getComponents() const
		{
			std::vector<T*> found;
			for (const std::unique_ptr<Components::Component>& pComponent : mv_components)
			{
				if (T* pFound = dynamic_cast<T*>(pComponent.get()))
				{
					found.push_back(pFound);
				}
			}
			return found;
		}

		// returns nullptr if the object already has one and the type only allows one
		template<typename T>
		T* addComponent()
		{
			std::unique_ptr<T> pComponent = std::make_unique<T>();
			if (!pComponent->allowMultiple() && getComponent<T>() != nullptr)
			{
				return nullptr;
			}

			T* pRaw = pComponent.get();
			pRaw->setOwner(this);
			pRaw->init();
			mv_components.push_back(std::move(pComponent));
			return pRaw;
		}

		void removeComponent(Components::Component* pComponent);
		const std::vector<std::unique_ptr<Components::Component>>& getComponents() const { return mv_components; }

		void displayImGuiAttributes();
	private:
		void displayComponents();

		void updateModelMatrix();

		glm::vec3 mv3_position{};
		glm::vec3 mv3_rotation{};
		glm::vec3 mv3_scale = glm::vec3(1);
		glm::quat m_rotationQuat = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		glm::mat4 m_modelMatrix = glm::mat4(1);
		glm::mat4 m_inverseModelMatrix{};

		std::string m_name;

		std::vector<std::unique_ptr<Components::Component>> mv_components;

		Craig::Scene* mp_scene = nullptr;

		//TODO: Maybe some sort of UUID system? Currently relying on either the name or the pointer, either of which could easily mess up


	};



}