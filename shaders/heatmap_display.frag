#version 330 core

in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D heatmapTexture;
uniform float maxRadiationWhM2;

vec3 HeatmapColor(float t)
{
    t = clamp(t, 0.0, 1.0);

    if (t < 0.25)
        return mix(vec3(0.02, 0.08, 0.65),
                   vec3(0.0, 0.85, 1.0), t / 0.25);

    if (t < 0.50)
        return mix(vec3(0.0, 0.85, 1.0),
                   vec3(0.05, 0.85, 0.20), (t - 0.25) / 0.25);

    if (t < 0.75)
        return mix(vec3(0.05, 0.85, 0.20),
                   vec3(1.0, 0.90, 0.0), (t - 0.50) / 0.25);

    return mix(vec3(1.0, 0.90, 0.0),
               vec3(0.90, 0.05, 0.02), (t - 0.75) / 0.25);
}

void main()
{
    float radiation = max(texture(heatmapTexture, TexCoords).r, 0.0);

    float normalized = clamp(
        radiation / max(maxRadiationWhM2, 1.0),
        0.0,
        1.0
    );

    float displayValue = normalized;

    FragColor = vec4(HeatmapColor(displayValue), 0.82);
}
