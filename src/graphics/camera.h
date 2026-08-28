#pragma once

#include <glm/glm.hpp>

/**
 * @brief A first-person camera providing view/projection matrices, movement, rotation and zoom
 *
 */
class Camera
{
public:
    Camera(glm::vec3 position);

    void move(glm::vec3 delta);
    void setPos(glm::vec3 pos);
    void setAspectRatio(float aspectRatio);
    void rotate(float xOffset, float yOffset);
    void zoom(float scrollOffset);

    glm::vec3 getPos() const;
    glm::vec3 getFront() const;
    glm::vec3 getRight() const;
    glm::vec3 getUp() const;
    float getFOV() const;
    float getZNear() const;
    float getZFar() const;
    float getAspectRatio() const;

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;

private:
    void updateVectors();
    float getYaw() const;
    float getPitch() const;

    glm::vec3 _pos;
    glm::vec3 _front;
    glm::vec3 _up;
    glm::vec3 _right;
    glm::vec3 _worldUp;

    float _yaw;
    float _pitch;

    float _zNear;
    float _zFar;
    int _fovDeg = 70;
    float _aspectRatio = 1.0f;

    float _sens;

    int MIN_FOV = 10;
    int MAX_FOV = 130; // default FOV, also the "fully zoomed out" value
    float ZOOM_STEP = 3.0f;
};