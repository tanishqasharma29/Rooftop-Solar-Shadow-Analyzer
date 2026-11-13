
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Movement directions.
enum class CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class Camera
{
public:

    // Initial camera position.
    glm::vec3 Position;

    // Camera coordinate directions.
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // Camera orientation.
    float Yaw;
    float Pitch;

    // Camera movement settings.
    float MovementSpeed;
    float MouseSensitivity;
    float Zoom;

    Camera(
        glm::vec3 position = glm::vec3(4.0f, 3.0f, 7.0f),
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
        float yaw = -90.0f,
        float pitch = -15.0f
    );

    // Returns the camera view matrix.
    glm::mat4 GetViewMatrix();

    // Processes WASD movement.
    void ProcessKeyboard(
        CameraMovement direction,
        float deltaTime
    );

    // Processes mouse movement.
    void ProcessMouseMovement(
        float xOffset,
        float yOffset,
        bool constrainPitch = true
    );

    // Processes mouse scroll.
    void ProcessMouseScroll(float yOffset);

private:

    // Recalculate Front, Right and Up vectors.
    void updateCameraVectors();
};
