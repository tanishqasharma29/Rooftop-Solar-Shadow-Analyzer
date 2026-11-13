
#version 330 core

layout(location = 0) in vec3 aPos;

// Object transformation.
uniform mat4 model;

// Transformation from world space to light clip space.
uniform mat4 lightSpaceMatrix;

void main()
{
    // Transform object-space position into world space,
    // then into the Sun's orthographic projection.
    gl_Position =
        lightSpaceMatrix *
        model *
        vec4(aPos, 1.0);
}
