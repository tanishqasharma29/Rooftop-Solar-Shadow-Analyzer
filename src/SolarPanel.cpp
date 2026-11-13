#include "SolarPanel.hpp"
#include "Shader.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

namespace
{
    void AddQuad(
        std::vector<Vertex>& vertices,
        std::vector<unsigned int>& indices,
        float minX,
        float maxX,
        float minZ,
        float maxZ,
        float y
    )
    {
        unsigned int start =
            static_cast<unsigned int>(vertices.size());

        const glm::vec3 normal(0.0f, 1.0f, 0.0f);

        vertices.push_back({glm::vec3(minX, y, minZ), normal});
        vertices.push_back({glm::vec3(maxX, y, minZ), normal});
        vertices.push_back({glm::vec3(maxX, y, maxZ), normal});
        vertices.push_back({glm::vec3(minX, y, maxZ), normal});

        indices.push_back(start);
        indices.push_back(start + 2);
        indices.push_back(start + 1);
        indices.push_back(start);
        indices.push_back(start + 3);
        indices.push_back(start + 2);
    }

    // Ray-AABB intersection. Returns true when a building blocks
    // the ray travelling from a panel toward the Sun.
    bool RayIntersectsAABB(
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const glm::vec3& boxMin,
        const glm::vec3& boxMax
    )
    {
        float tMin = 0.001f;
        float tMax = 10000.0f;

        for (int axis = 0; axis < 3; ++axis)
        {
            const float origin = rayOrigin[axis];
            const float direction = rayDirection[axis];

            if (std::abs(direction) < 1e-6f)
            {
                if (origin < boxMin[axis] || origin > boxMax[axis])
                    return false;
                continue;
            }

            float t1 = (boxMin[axis] - origin) / direction;
            float t2 = (boxMax[axis] - origin) / direction;

            if (t1 > t2)
                std::swap(t1, t2);

            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);

            if (tMax < tMin)
                return false;
        }

        return tMax > 0.001f;
    }

    bool IsBlockedByNeighbour(const glm::vec3& origin, const glm::vec3& direction)
    {
        // Neighbour 1: position (3.5, 0, 0), scale (0.65, 0.75, 0.65).
        const glm::vec3 neighbour1Min(2.85f, 0.0f, -0.65f);
        const glm::vec3 neighbour1Max(4.15f, 1.50f, 0.65f);

        // Neighbour 2: current diagnostic test building.
        // Position (-1.8, 0, -1), scale (0.75, 2.5, 0.75).
        const glm::vec3 neighbour2Min(-2.55f, 0.0f, -1.75f);
        const glm::vec3 neighbour2Max(-1.05f, 5.00f, -0.25f);

        return
            RayIntersectsAABB(origin, direction, neighbour1Min, neighbour1Max) ||
            RayIntersectsAABB(origin, direction, neighbour2Min, neighbour2Max);
    }
}

SolarPanel::SolarPanel()
{
    CreatePanelGeometry();
}

void SolarPanel::CreatePanelGeometry()
{
    std::vector<Vertex> baseVertices;
    std::vector<unsigned int> baseIndices;

    AddQuad(
        baseVertices, baseIndices,
        -0.465f, 0.465f,
        -0.465f, 0.465f,
        0.006f
    );

    panelBaseMesh = std::make_unique<Mesh>(
        baseVertices, baseIndices
    );

    std::vector<Vertex> frameVertices;
    std::vector<unsigned int> frameIndices;

    constexpr float outer = 0.5f;
    constexpr float inner = 0.445f;
    constexpr float frameY = 0.014f;

    AddQuad(frameVertices, frameIndices,
        -outer, outer, -outer, -inner, frameY);

    AddQuad(frameVertices, frameIndices,
        -outer, outer, inner, outer, frameY);

    AddQuad(frameVertices, frameIndices,
        -outer, -inner, -inner, inner, frameY);

    AddQuad(frameVertices, frameIndices,
        inner, outer, -inner, inner, frameY);

    panelFrameMesh = std::make_unique<Mesh>(
        frameVertices, frameIndices
    );

    std::vector<Vertex> gridVertices;
    std::vector<unsigned int> gridIndices;

    constexpr float gridY = 0.021f;
    constexpr float lineWidth = 0.006f;

    for (int i = 1; i <= 5; ++i)
    {
        const float x = -0.465f + i * (0.93f / 6.0f);

        AddQuad(
            gridVertices, gridIndices,
            x - lineWidth * 0.5f,
            x + lineWidth * 0.5f,
            -0.445f, 0.445f,
            gridY
        );
    }

    for (int i = 1; i <= 3; ++i)
    {
        const float z = -0.445f + i * (0.89f / 4.0f);

        AddQuad(
            gridVertices, gridIndices,
            -0.445f, 0.445f,
            z - lineWidth * 0.5f,
            z + lineWidth * 0.5f,
            gridY
        );
    }

    panelGridMesh = std::make_unique<Mesh>(
        gridVertices, gridIndices
    );

    std::cout << "Realistic solar panel geometry created!" << std::endl;
}

void SolarPanel::GenerateLayout(
    float roofWidthValue,
    float roofDepthValue,
    float panelWidthValue,
    float panelDepthValue,
    float spacingValue
)
{
    panelTransforms.clear();
    panelExposure.clear();
    panelCount = 0;
    totalArea = 0.0f;

    if (
        roofWidthValue <= 0.0f ||
        roofDepthValue <= 0.0f ||
        panelWidthValue <= 0.0f ||
        panelDepthValue <= 0.0f ||
        spacingValue < 0.0f
    )
    {
        std::cerr << "Invalid solar panel layout dimensions." << std::endl;
        return;
    }

    roofWidth = roofWidthValue;
    roofDepth = roofDepthValue;
    panelWidth = panelWidthValue;
    panelDepth = panelDepthValue;
    panelSpacing = spacingValue;

    const int columns = static_cast<int>(
        std::floor((roofWidth + panelSpacing) / (panelWidth + panelSpacing))
    );

    const int rows = static_cast<int>(
        std::floor((roofDepth + panelSpacing) / (panelDepth + panelSpacing))
    );

    if (columns <= 0 || rows <= 0)
    {
        std::cout << "No solar panels fit on the rooftop." << std::endl;
        return;
    }

    panelCount = rows * columns;
    totalArea = panelCount * panelWidth * panelDepth;

    RebuildTransforms();

    std::cout
        << "\nSolar panel layout generated."
        << "\nRows: " << rows
        << "\nColumns: " << columns
        << "\nTotal panels: " << panelCount
        << "\nTotal panel area: " << totalArea
        << " square units"
        << std::endl;
}

void SolarPanel::SetTiltAngle(float degrees)
{
    tiltAngleDegrees = std::clamp(degrees, 0.0f, 45.0f);
    RebuildTransforms();
}

float SolarPanel::GetTiltAngle() const
{
    return tiltAngleDegrees;
}

void SolarPanel::RebuildTransforms()
{
    panelTransforms.clear();
    panelExposure.clear();

    const int columns = static_cast<int>(
        std::floor((roofWidth + panelSpacing) / (panelWidth + panelSpacing))
    );

    const int rows = static_cast<int>(
        std::floor((roofDepth + panelSpacing) / (panelDepth + panelSpacing))
    );

    if (columns <= 0 || rows <= 0)
        return;

    const float usedWidth =
        columns * panelWidth + (columns - 1) * panelSpacing;

    const float usedDepth =
        rows * panelDepth + (rows - 1) * panelSpacing;

    const float startX = -usedWidth * 0.5f + panelWidth * 0.5f;
    const float startZ = -usedDepth * 0.5f + panelDepth * 0.5f;

    constexpr float roofHeight = 2.0f;
    constexpr float panelOffset = 0.035f;

    const float angleRadians = glm::radians(tiltAngleDegrees);
    const float tiltLift = 0.5f * panelDepth * std::sin(angleRadians);

    for (int row = 0; row < rows; ++row)
    {
        for (int column = 0; column < columns; ++column)
        {
            const float x = startX + column * (panelWidth + panelSpacing);
            const float z = startZ + row * (panelDepth + panelSpacing);

            glm::mat4 model(1.0f);

            model = glm::translate(
                model,
                glm::vec3(x, roofHeight + panelOffset + tiltLift, z)
            );

            model = glm::rotate(
                model,
                -angleRadians,
                glm::vec3(1.0f, 0.0f, 0.0f)
            );

            model = glm::scale(
                model,
                glm::vec3(panelWidth, 1.0f, panelDepth)
            );

            panelTransforms.push_back(model);

            PanelExposure exposure;
            exposure.center = glm::vec3(
                model * glm::vec4(0.0f, 0.015f, 0.0f, 1.0f)
            );
            panelExposure.push_back(exposure);
        }
    }
}

void SolarPanel::UpdateSunExposure(
    const glm::vec3& sunDirection,
    bool sunAboveHorizon
)
{
    if (panelExposure.size() != panelTransforms.size())
        return;

    if (!sunAboveHorizon || glm::length(sunDirection) < 1e-6f)
    {
        for (auto& panel : panelExposure)
        {
            panel.sunlightPercent = 0.0f;
            panel.shaded = true;
        }
        return;
    }

    const glm::vec3 lightDirection = glm::normalize(sunDirection);

    const float angle = glm::radians(tiltAngleDegrees);
    const glm::vec3 panelNormal(
        0.0f,
        std::cos(angle),
        -std::sin(angle)
    );

    const float incidence = std::clamp(
        glm::dot(panelNormal, lightDirection),
        0.0f,
        1.0f
    );

    // M9: Use a denser 7 x 7 grid on every panel. The sample
    // points are transformed into world space, so each panel gets
    // its own obstruction result rather than sharing a roof-wide value.
    constexpr int samplesPerAxis = 7;
    constexpr int totalSamples =
        samplesPerAxis * samplesPerAxis;

    for (std::size_t panelIndex = 0;
         panelIndex < panelExposure.size();
         ++panelIndex)
    {
        int visibleSamples = 0;
        const glm::mat4& model = panelTransforms[panelIndex];

        for (int row = 0; row < samplesPerAxis; ++row)
        {
            for (int column = 0; column < samplesPerAxis; ++column)
            {
                const float sampleU =
                    (static_cast<float>(column) + 0.5f) /
                    static_cast<float>(samplesPerAxis);

                const float sampleV =
                    (static_cast<float>(row) + 0.5f) /
                    static_cast<float>(samplesPerAxis);

                const float localX = -0.445f + 0.89f * sampleU;
                const float localZ = -0.445f + 0.89f * sampleV;

                const glm::vec4 worldPoint =
                    model * glm::vec4(localX, 0.015f, localZ, 1.0f);

                const glm::vec3 rayOrigin =
                    glm::vec3(worldPoint) +
                    panelNormal * 0.025f +
                    lightDirection * 0.035f;

                if (!IsBlockedByNeighbour(rayOrigin, lightDirection))
                    ++visibleSamples;
            }
        }

        const float visibleFraction =
            static_cast<float>(visibleSamples) /
            static_cast<float>(totalSamples);

        panelExposure[panelIndex].shaded =
            (visibleSamples == 0 || incidence <= 0.001f);

        panelExposure[panelIndex].sunlightPercent =
            panelExposure[panelIndex].shaded
            ? 0.0f
            : incidence * visibleFraction * 100.0f;
    }
}

void SolarPanel::Draw(Shader& shader) const
{
    if (
        !panelBaseMesh ||
        !panelFrameMesh ||
        !panelGridMesh ||
        panelTransforms.empty()
    )
    {
        return;
    }

    shader.Use();
    float maxExposure = 0.0f;

for (const auto& panel : panelExposure)
{
    maxExposure = std::max(
        maxExposure,
        panel.sunlightPercent
    );
}

    for (std::size_t i = 0; i < panelTransforms.size(); ++i)
    {
        const glm::mat4& model = panelTransforms[i];

        shader.SetMat4("model", model);

        float exposure = 0.0f;

if (i < panelExposure.size() && maxExposure > 0.001f)
{
    exposure = std::clamp(
        panelExposure[i].sunlightPercent / maxExposure,
        0.0f,
        1.0f
    );
}

glm::vec3 panelColor;

if (exposure < 0.33f)
{
    panelColor = glm::mix(
        glm::vec3(0.05f, 0.15f, 0.95f),
        glm::vec3(0.0f, 0.95f, 0.35f),
        exposure / 0.33f
    );
}
else if (exposure < 0.66f)
{
    panelColor = glm::mix(
        glm::vec3(0.0f, 0.95f, 0.35f),
        glm::vec3(1.0f, 0.9f, 0.0f),
        (exposure - 0.33f) / 0.33f
    );
}
else
{
    panelColor = glm::mix(
        glm::vec3(1.0f, 0.9f, 0.0f),
        glm::vec3(1.0f, 0.05f, 0.0f),
        (exposure - 0.66f) / 0.34f
    );
}

        // Individual panel heatmap colour.
        shader.SetVec3("objectColor", panelColor);
        panelBaseMesh->Draw();

        // Panel frame.
        shader.SetVec3(
            "objectColor",
            glm::vec3(0.65f, 0.68f, 0.72f)
        );
        panelFrameMesh->Draw();

        // Panel grid.
        shader.SetVec3(
            "objectColor",
            glm::vec3(0.25f, 0.65f, 0.90f)
        );
        panelGridMesh->Draw();
    }
}

int SolarPanel::GetPanelCount() const
{
    return panelCount;
}

float SolarPanel::GetTotalArea() const
{
    return totalArea;
}

int SolarPanel::GetSunlitPanelCount() const
{
    int count = 0;
    for (const auto& panel : panelExposure)
        if (!panel.shaded && panel.sunlightPercent > 0.0f)
            ++count;
    return count;
}

int SolarPanel::GetShadedPanelCount() const
{
    return panelCount - GetSunlitPanelCount();
}

float SolarPanel::GetAverageSunlightPercent() const
{
    if (panelExposure.empty())
        return 0.0f;

    float total = 0.0f;
    for (const auto& panel : panelExposure)
        total += panel.sunlightPercent;

    return total / static_cast<float>(panelExposure.size());
}

const std::vector<PanelExposure>& SolarPanel::GetPanelExposure() const
{
    return panelExposure;
}
