
#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

// Sun's light-space transformation.
uniform mat4 lightSpaceMatrix;

out vec3 FragPos;
out vec3 Normal;

// Position used for shadow-map lookup.
out vec4 FragPosLightSpace;

void main()
{
    // Transform vertex into world coordinates.
    vec4 worldPosition =
        model * vec4(aPos, 1.0);

    FragPos = worldPosition.xyz;

    // Inverse-transpose normal transformation.
    Normal =
        mat3(transpose(inverse(model))) * aNormal;

    // Convert world position to light clip coordinates.
    FragPosLightSpace =
        lightSpaceMatrix * worldPosition;

    // Normal camera projection.
    gl_Position =
        projection *
        view *
        worldPosition;
}
