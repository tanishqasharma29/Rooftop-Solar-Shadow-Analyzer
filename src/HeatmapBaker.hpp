#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

class Shader;
class ShadowMap;

struct HeatmapStatistics
{
    float minimum = 0.0f;
    float maximum = 0.0f;
    double average = 0.0;
    double total = 0.0;
    int pixelCount = 0;
};

class HeatmapBaker
{
public:
    static constexpr int RESOLUTION = 128;
    static constexpr float ROOF_MIN_X = -1.0f;
    static constexpr float ROOF_MAX_X =  1.0f;
    static constexpr float ROOF_MIN_Z = -1.0f;
    static constexpr float ROOF_MAX_Z =  1.0f;
    static constexpr float ROOF_Y = 2.01f;

    HeatmapBaker();
    ~HeatmapBaker();

    HeatmapBaker(const HeatmapBaker&) = delete;
    HeatmapBaker& operator=(const HeatmapBaker&) = delete;

    void Clear();

    void Accumulate(
        Shader& shader,
        const ShadowMap& shadowMap,
        const glm::mat4& lightSpaceMatrix,
        const glm::vec3& sunDirection,
        float intervalHours,
        float directRadiation,
        float diffuseRadiation
    );

    void Draw(
        Shader& shader,
        const glm::mat4& view,
        const glm::mat4& projection
    ) const;

    HeatmapStatistics GetStatistics() const;
    void PrintStatistics() const;

private:
    GLuint framebuffer = 0;
    GLuint heatmapTexture = 0;
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    void InitializeFramebuffer();
    void InitializeRoofGeometry();
};
