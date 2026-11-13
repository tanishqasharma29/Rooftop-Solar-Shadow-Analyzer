#version 330 core

in vec3 WorldPosition;
out vec4 FragColor;

uniform sampler2D shadowMap;
uniform mat4 lightSpaceMatrix;
uniform vec3 sunDirection;
uniform float intervalHours;
uniform float directRadiation;
uniform float diffuseRadiation;

float CalculateVisibility(vec3 worldPosition)
{
    vec4 lightPosition = lightSpaceMatrix * vec4(worldPosition, 1.0);
    if (lightPosition.w <= 0.0) return 0.0;

    vec3 p = lightPosition.xyz / lightPosition.w;
    p = p * 0.5 + 0.5;

    if (p.x < 0.0 || p.x > 1.0 ||
        p.y < 0.0 || p.y > 1.0 ||
        p.z < 0.0 || p.z > 1.0)
        return 1.0;

    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float visible = 0.0;
    const float bias = 0.0015;

    for (int x = -1; x <= 1; ++x)
    for (int y = -1; y <= 1; ++y)
    {
        float depth = texture(shadowMap, p.xy + vec2(x, y) * texel).r;
        visible += (p.z - bias <= depth) ? 1.0 : 0.0;
    }
    return visible / 9.0;
}

void main()
{
    vec3 roofNormal = vec3(0.0, 1.0, 0.0);
    vec3 toSun = normalize(sunDirection);
    float incidence = max(dot(roofNormal, toSun), 0.0);
    float visibility = CalculateVisibility(WorldPosition);

    float direct = max(directRadiation, 0.0) * incidence * visibility;
    float diffuse = max(diffuseRadiation, 0.0) * 0.35;
    float contribution = (direct + diffuse) * max(intervalHours, 0.0);

    // Additive blending accumulates irradiance × hours in the R32F texture.
    FragColor = vec4(contribution, 0.0, 0.0, 1.0);
}
