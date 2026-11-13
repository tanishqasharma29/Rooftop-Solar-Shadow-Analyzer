
#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

uniform vec3 viewPos;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 objectColor;

// Shadow map.
uniform sampler2D shadowMap;

// Shadow controls.
uniform bool shadowsEnabled;
uniform bool pcfEnabled;

// --------------------------------------------------
// SHADOW CALCULATION
// --------------------------------------------------

float CalculateShadow(
    vec4 lightSpacePosition,
    vec3 normal,
    vec3 lightDirection
)
{
    // If shadows are disabled, return fully lit.
    if (!shadowsEnabled)
        return 0.0;

    // Perspective divide.
    vec3 projected =
        lightSpacePosition.xyz /
        lightSpacePosition.w;

    // Transform from NDC [-1,1] to texture [0,1].
    projected = projected * 0.5 + 0.5;

    // Outside light frustum = no shadow.
    if (
        projected.x < 0.0 || projected.x > 1.0 ||
        projected.y < 0.0 || projected.y > 1.0 ||
        projected.z < 0.0 || projected.z > 1.0
    )
    {
        return 0.0;
    }

    // Depth stored in shadow map.
    float currentDepth = projected.z;

    // ------------------------------------------
    // BIAS TO REDUCE SHADOW ACNE
    // ------------------------------------------

    float NdotL = max(
        dot(normal, lightDirection),
        0.0
    );

    // Slope-scaled bias:
    // Surfaces at grazing angles receive larger bias.
    float bias = max(
        0.00015 * (1.0 - NdotL),
        0.000025
    );

    // ------------------------------------------
    // SINGLE SAMPLE SHADOW
    // ------------------------------------------

    if (!pcfEnabled)
    {
        float closestDepth = texture(
            shadowMap,
            projected.xy
        ).r;

        return
            currentDepth - bias > closestDepth
            ? 1.0
            : 0.0;
    }

    // ------------------------------------------
    // PCF 3 x 3 FILTER
    // ------------------------------------------

    vec2 texelSize =
        1.0 / vec2(textureSize(shadowMap, 0));

    float shadow = 0.0;

    // Nine neighbouring depth comparisons.
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 offset =
                vec2(float(x), float(y)) * texelSize;

            float closestDepth = texture(
                shadowMap,
                projected.xy + offset
            ).r;

            shadow +=
                currentDepth - bias > closestDepth
                ? 1.0
                : 0.0;
        }
    }

    // Average the nine comparisons.
    shadow /= 9.0;

    return shadow;
}

// --------------------------------------------------
// MAIN LIGHTING
// --------------------------------------------------

void main()
{
    vec3 norm = normalize(Normal);

    // Direction towards Sun.
    vec3 lightDirection = normalize(lightDir);

    // Direction towards camera.
    vec3 viewDirection =
        normalize(viewPos - FragPos);

    // ------------------------------------------
    // AMBIENT
    // ------------------------------------------

    vec3 ambient =
        0.28 * lightColor;

    // ------------------------------------------
    // DIFFUSE
    // ------------------------------------------

    float diffuseFactor = max(
        dot(norm, lightDirection),
        0.0
    );

    vec3 diffuse =
        diffuseFactor * lightColor;

    // ------------------------------------------
    // SPECULAR (BLINN-PHONG)
    // ------------------------------------------

    vec3 halfwayDirection = normalize(
        lightDirection + viewDirection
    );

    float specularFactor = pow(
        max(
            dot(norm, halfwayDirection),
            0.0
        ),
        32.0
    );

    vec3 specular =
        0.35 * specularFactor * lightColor;

    // ------------------------------------------
    // SHADOW
    // ------------------------------------------

    float shadow = CalculateShadow(
        FragPosLightSpace,
        norm,
        lightDirection
    );

    // Ambient remains visible in shadow.
    // Direct diffuse and specular light are reduced.
    vec3 result =
        ambient +
        (1.0 - shadow) * (diffuse + specular);

    result *= objectColor;

    FragColor = vec4(result, 1.0);
}
