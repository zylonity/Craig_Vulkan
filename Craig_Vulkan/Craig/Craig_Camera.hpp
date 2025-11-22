#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL.h>

#include "Craig_Constants.hpp"

namespace Craig {

    class Camera
    {
    public:
        Craig::Camera(glm::vec3 pos = glm::vec3(0.0f));

        void update(const float& deltaTime);
        void processSDLEvent(SDL_Event& e);

        void panTilt(float pan, float tilt);
        void slew(const glm::vec3& v);
        void raise(float f);

        glm::mat4 getView()  const { return m_view; }
        glm::mat4 getProj()  const { return m_proj; }

        glm::vec3& getPosition() { return m_position; }
        glm::vec3& getVelocity() { return m_velocity; }
        glm::vec2& getRotation() { return m_pitchYaw; }

        float m_fov = 45.0f;
        float m_nearPlane = 0.1f;
        float m_farPlane = 100.0f;
        float m_aspect = 1.0f;  // set this from swapchain

        float m_movementSpeed = 1.0f;
        float m_rotSpeed = 1.0f;

        void setPosition(const glm::vec3& p) { m_position = p; }
        void setPitchYaw(const glm::vec2& py) { m_pitchYaw = py; }

    private:
        void updateView(const float& deltaTime);
        void updateProj();

        glm::vec3 forward() const;
        glm::vec3 right() const;
        glm::vec3 up() const;

        glm::vec3 m_position{};
        glm::vec3 m_velocity{};
        glm::vec2 m_pitchYaw{ 0.0f, 0.0f }; // x = pitch, y = yaw

        glm::mat4 m_view;
        glm::mat4 m_proj;

        glm::mat4 getRotationMatrix();
    };
}

