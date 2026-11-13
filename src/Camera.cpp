
#include "Camera.hpp"
#include <cmath>
#include <algorithm>

Camera::Camera(
    glm::vec3 position,
    glm::vec3 up,
    float yaw,
    float pitch
)
{
    Position = position;
    WorldUp = up;

    Yaw = yaw;
    Pitch = pitch;

    MovementSpeed = 5.0f;
    MouseSensitivity = 0.1f;
    Zoom = 45.0f;

    updateCameraVectors();
}

glm::mat4 Camera::GetViewMatrix()
{
    // Look from camera position towards its Front direction.
    return glm::lookAt(
        Position,
        Position + Front,
        Up
    );
}

void Camera::ProcessKeyboard(
    CameraMovement direction,
    float deltaTime
)
{
    float velocity = MovementSpeed * deltaTime;

    if (direction == CameraMovement::FORWARD)
        Position += Front * velocity;

    if (direction == CameraMovement::BACKWARD)
        Position -= Front * velocity;

    if (direction == CameraMovement::LEFT)
        Position -= Right * velocity;

    if (direction == CameraMovement::RIGHT)
        Position += Right * velocity;

    if (direction == CameraMovement::UP)
        Position += WorldUp * velocity;

    if (direction == CameraMovement::DOWN)
        Position -= WorldUp * velocity;
}

void Camera::ProcessMouseMovement(
    float xOffset,
    float yOffset,
    bool constrainPitch
)
{
    xOffset *= MouseSensitivity;
    yOffset *= MouseSensitivity;

    Yaw += xOffset;
    Pitch += yOffset;

    // Prevent the camera from flipping upside down.
    if (constrainPitch)
    {
        Pitch = std::clamp(Pitch, -89.0f, 89.0f);
    }

    updateCameraVectors();
}

void Camera::ProcessMouseScroll(float yOffset)
{
    Zoom -= yOffset;

    Zoom = std::clamp(Zoom, 1.0f, 75.0f);
}

void Camera::updateCameraVectors()
{
    // Convert spherical yaw/pitch angles into a direction vector.
    glm::vec3 front;

    front.x = cos(glm::radians(Yaw))
            * cos(glm::radians(Pitch));

    front.y = sin(glm::radians(Pitch));

    front.z = sin(glm::radians(Yaw))
            * cos(glm::radians(Pitch));

    // Normalize direction to maintain consistent movement.
    Front = glm::normalize(front);

    // Right is perpendicular to Front and WorldUp.
    Right = glm::normalize(
        glm::cross(Front, WorldUp)
    );

    // Corrected Up vector.
    Up = glm::normalize(
        glm::cross(Right, Front)
    );
}
