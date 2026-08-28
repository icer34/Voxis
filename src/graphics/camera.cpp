#include "camera.h"

#include <cmath>
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(glm::vec3 position)
    : _pos(position),
      _worldUp(0.0f, 1.0f, 0.0f),
      _yaw(-90.0f),
      _pitch(0.0f),
      _zNear(0.1f),
      _zFar(500.0f),
      _fovDeg(70.0f),
      _sens(0.1f)
{
    updateVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(_pos, _pos + _front, _up);
}

glm::mat4 Camera::getProjectionMatrix() const
{
    glm::mat4 proj = glm::perspective(glm::radians(float(_fovDeg)), _aspectRatio, _zNear, _zFar);
    // GLM assumes OpenGL's NDC (Y up); Vulkan's NDC has Y pointing down.
    proj[1][1] *= -1.0f;
    return proj;
}

void Camera::move(glm::vec3 delta)
{
    _pos += delta;
}

void Camera::setPos(glm::vec3 pos)
{
    _pos = pos;
}

void Camera::setAspectRatio(float aspectRatio)
{
    _aspectRatio = aspectRatio;
}

void Camera::rotate(float xOffset, float yOffset)
{
    _yaw += xOffset * _sens;
    // clamp yaw so it doesnt accumulate to infinity
    _yaw = std::fmod(_yaw, 360.0f);
    _pitch += yOffset * _sens;

    // avoid camera flip
    _pitch = std::clamp(_pitch, -89.0f, 89.0f);

    updateVectors();
}

void Camera::zoom(float scrollOffset)
{
    // scroll up (positive offset) zooms in -> smaller FOV
    _fovDeg -= static_cast<int>(scrollOffset * ZOOM_STEP);
    _fovDeg = std::clamp(_fovDeg, MIN_FOV, MAX_FOV);
}

glm::vec3 Camera::getPos() const
{
    return _pos;
}

glm::vec3 Camera::getFront() const
{
    return _front;
}

glm::vec3 Camera::getRight() const
{
    return _right;
}

glm::vec3 Camera::getUp() const
{
    return _up;
}

float Camera::getFOV() const
{
    return float(_fovDeg);
}

float Camera::getZNear() const
{
    return _zNear;
}

float Camera::getZFar() const
{
    return _zFar;
}

float Camera::getAspectRatio() const
{
    return _aspectRatio;
}

float Camera::getYaw() const
{
    return _yaw;
}

float Camera::getPitch() const
{
    return _pitch;
}

void Camera::updateVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(_yaw)) * cos(glm::radians(_pitch));
    front.y = sin(glm::radians(_pitch));
    front.z = sin(glm::radians(_yaw)) * cos(glm::radians(_pitch));

    _front = glm::normalize(front);
    _right = glm::normalize(glm::cross(_front, _worldUp));
    _up = glm::normalize(glm::cross(_right, _front));
}