#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL

#include <cmath>

#include "Craig_Camera.hpp"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

Craig::Camera::Camera(glm::vec3 pos)
    : m_position(pos)
{
}

void Craig::Camera::update(const float& deltaTime)
{

    updateView(deltaTime);
    updateProj();
}

void Craig::Camera::updateView(const float& deltaTime)
{
    
    m_position += ((forward() * m_velocity.z) * m_movementSpeed) * deltaTime;
    m_position += ((right() * m_velocity.x) * m_movementSpeed) * deltaTime;

    glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), m_position);
    glm::mat4 cameraRotation = getRotationMatrix();
    m_view = glm::inverse(cameraTranslation * cameraRotation);

}

void Craig::Camera::updateProj()
{
    m_proj = glm::perspective(glm::radians(m_fov), m_aspect, m_nearPlane, m_farPlane);
    m_proj[1][1] *= -1.0f; // Vulkan flip
}

void Craig::Camera::panTilt(float pan, float tilt)
{
    m_pitchYaw.x -= tilt;
    m_pitchYaw.y -= pan;
}

glm::vec3 Craig::Camera::forward() const
{
    float pitchRad = glm::radians(m_pitchYaw.x);
    float yawRad = glm::radians(m_pitchYaw.y);

    return glm::vec3(
        std::sin(yawRad) * std::cos(pitchRad),
        -std::sin(pitchRad),
        std::cos(yawRad) * std::cos(pitchRad)
    );
}

glm::vec3 Craig::Camera::right() const
{
    float yawRad = glm::radians(m_pitchYaw.y);
    return glm::vec3(std::cos(yawRad), 0.0f, -std::sin(yawRad));
}

glm::vec3 Craig::Camera::up() const
{
    float pitchRad = glm::radians(m_pitchYaw.x);
    float yawRad = glm::radians(m_pitchYaw.y);

    float cosPitch = std::cos(pitchRad);
    float sinPitch = std::sin(pitchRad);
    float cosYaw = std::cos(yawRad);
    float sinYaw = std::sin(yawRad);

    return glm::vec3(
        sinYaw * sinPitch,
        cosPitch,
        cosYaw * sinPitch
    );
}

void Craig::Camera::slew(const glm::vec3& v)
{
    m_position += -forward() * v.z + right() * v.x;
}

void Craig::Camera::raise(float f)
{
    m_position += up() * f;
}

glm::mat4 Craig::Camera::getRotationMatrix()
{
    glm::quat pitchRotation = glm::angleAxis(glm::radians(m_pitchYaw[0]), glm::vec3{1.f, 0.f, 0.f});
    glm::quat yawRotation = glm::angleAxis(glm::radians(m_pitchYaw[1]), glm::vec3{0.f, 1.f, 0.f});

    return (glm::toMat4(yawRotation) * glm::toMat4(pitchRotation));

}

void Craig::Camera::processSDLEvent(SDL_Event& e) {


    if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_w) { m_velocity.z = -1; }
            if (e.key.keysym.sym == SDLK_s) { m_velocity.z = 1; }
            if (e.key.keysym.sym == SDLK_a) { m_velocity.x = -1; }
            if (e.key.keysym.sym == SDLK_d) { m_velocity.x = 1; }
        }

        if (e.type == SDL_KEYUP) {
            if (e.key.keysym.sym == SDLK_w) { m_velocity.z = 0; }
            if (e.key.keysym.sym == SDLK_s) { m_velocity.z = 0; }
            if (e.key.keysym.sym == SDLK_a) { m_velocity.x = 0; }
            if (e.key.keysym.sym == SDLK_d) { m_velocity.x = 0; }
        }

        if (e.type == SDL_MOUSEMOTION) {
            m_pitchYaw[1] -= ((float)e.motion.xrel / 200.f) * m_rotSpeed;
            m_pitchYaw[0] -= ((float)e.motion.yrel / 200.f) * m_rotSpeed;
        }
    }
    

}