
#version 330 core

layout(location = 0) in vec3 aPos;

// Transformation matrices.
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Object space -> World space -> View space -> Clip space.
    gl_Position =
        projection *
        view *
        model *
        vec4(aPos, 1.0);
}
