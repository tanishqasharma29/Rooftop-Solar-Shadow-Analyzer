#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec2 aTexCoords;

out vec3 WorldPosition;

void main()
{
    // Preserve the actual rooftop world position for shadow calculations.
    WorldPosition = aPosition;

    // Bake the rooftop into the heatmap using its UV coordinates.
    // This makes each texture pixel correspond to a rooftop location.
    vec2 uvPosition = aTexCoords * 2.0 - 1.0;

    gl_Position = vec4(uvPosition, 0.0, 1.0);
}
