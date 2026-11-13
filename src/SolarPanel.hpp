#pragma once

#include "Mesh.hpp"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;

struct PanelExposure
{
    glm::vec3 center{0.0f};
    float sunlightPercent = 0.0f;
    bool shaded = true;
};

class SolarPanel
{
public:
    SolarPanel();

    SolarPanel(const SolarPanel&) = delete;
    SolarPanel& operator=(const SolarPanel&) = delete;

    void GenerateLayout(
        float roofWidth,
        float roofDepth,
        float panelWidth,
        float panelDepth,
        float spacing
    );

    void SetTiltAngle(float degrees);
    float GetTiltAngle() const;

    // Evaluate incidence and neighbour obstruction independently for every panel.
    // M9 uses a denser per-panel sample grid for partial-shadow estimation.
    void UpdateSunExposure(
        const glm::vec3& sunDirection,
        bool sunAboveHorizon
    );

    void Draw(Shader& shader) const;

    int GetPanelCount() const;
    float GetTotalArea() const;

    int GetSunlitPanelCount() const;
    int GetShadedPanelCount() const;
    float GetAverageSunlightPercent() const;
    const std::vector<PanelExposure>& GetPanelExposure() const;

private:
    std::unique_ptr<Mesh> panelBaseMesh;
    std::unique_ptr<Mesh> panelFrameMesh;
    std::unique_ptr<Mesh> panelGridMesh;

    std::vector<glm::mat4> panelTransforms;
    std::vector<PanelExposure> panelExposure;

    int panelCount = 0;
    float totalArea = 0.0f;
    float tiltAngleDegrees = 20.0f;

    float roofWidth = 2.0f;
    float roofDepth = 2.0f;
    float panelWidth = 0.40f;
    float panelDepth = 0.60f;
    float panelSpacing = 0.08f;

    void CreatePanelGeometry();
    void RebuildTransforms();
};
