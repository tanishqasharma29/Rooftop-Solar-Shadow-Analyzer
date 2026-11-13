#include "HeatmapBaker.hpp"

#include "Shader.hpp"
#include "ShadowMap.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <limits>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <cmath>

HeatmapBaker::HeatmapBaker()
{
    InitializeFramebuffer();
    InitializeRoofGeometry();
    Clear();

    std::cout << "Heatmap system initialized: "
              << RESOLUTION << " x " << RESOLUTION
              << std::endl;
}

void HeatmapBaker::InitializeFramebuffer()
{
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glGenTextures(1, &heatmapTexture);
    glBindTexture(GL_TEXTURE_2D, heatmapTexture);

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_R32F,
        RESOLUTION, RESOLUTION, 0,
        GL_RED, GL_FLOAT, nullptr
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, heatmapTexture, 0
    );

    GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuffer);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        throw std::runtime_error("Heatmap framebuffer creation failed");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void HeatmapBaker::InitializeRoofGeometry()
{
    const float y = ROOF_Y;

    const float vertices[] =
    {
        ROOF_MIN_X, y, ROOF_MIN_Z, 0.0f, 0.0f,
        ROOF_MAX_X, y, ROOF_MIN_Z, 1.0f, 0.0f,
        ROOF_MAX_X, y, ROOF_MAX_Z, 1.0f, 1.0f,
        ROOF_MIN_X, y, ROOF_MAX_Z, 0.0f, 1.0f
    };

    const GLuint indices[] = {0, 1, 2, 0, 2, 3};

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                          5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void HeatmapBaker::Clear()
{
    GLint previousFramebuffer = 0;
    GLint previousViewport[4] = {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, RESOLUTION, RESOLUTION);

    const GLfloat zero[] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearBufferfv(GL_COLOR, 0, zero);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    glViewport(previousViewport[0], previousViewport[1],
               previousViewport[2], previousViewport[3]);
}

void HeatmapBaker::Accumulate(
    Shader& shader,
    const ShadowMap& shadowMap,
    const glm::mat4& lightSpaceMatrix,
    const glm::vec3& sunDirection,
    float intervalHours,
    float directRadiation,
    float diffuseRadiation
)
{
    if (intervalHours <= 0.0f)
        return;

    GLint previousFramebuffer = 0;
    GLint previousViewport[4] = {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    const GLboolean previousDepthTest = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean previousBlend = glIsEnabled(GL_BLEND);
    GLint previousBlendSrcRGB;
GLint previousBlendDstRGB;
GLint previousBlendSrcAlpha;
GLint previousBlendDstAlpha;
GLint previousBlendEquationRGB;
GLint previousBlendEquationAlpha;

glGetIntegerv(GL_BLEND_SRC_RGB, &previousBlendSrcRGB);
glGetIntegerv(GL_BLEND_DST_RGB, &previousBlendDstRGB);
glGetIntegerv(GL_BLEND_SRC_ALPHA, &previousBlendSrcAlpha);
glGetIntegerv(GL_BLEND_DST_ALPHA, &previousBlendDstAlpha);

glGetIntegerv(
    GL_BLEND_EQUATION_RGB,
    &previousBlendEquationRGB
);

glGetIntegerv(
    GL_BLEND_EQUATION_ALPHA,
    &previousBlendEquationAlpha
);

    directRadiation = std::clamp(directRadiation, 0.0f, 1200.0f);
    diffuseRadiation = std::clamp(diffuseRadiation, 0.0f, 1200.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, RESOLUTION, RESOLUTION);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    shader.Use();
    shadowMap.BindForReading(1);

    glUniform1i(glGetUniformLocation(shader.ID, "shadowMap"), 1);
    glUniformMatrix4fv(
        glGetUniformLocation(shader.ID, "lightSpaceMatrix"),
        1, GL_FALSE, glm::value_ptr(lightSpaceMatrix)
    );
    glUniform3fv(
        glGetUniformLocation(shader.ID, "sunDirection"),
        1, glm::value_ptr(sunDirection)
    );
    glUniform1f(
        glGetUniformLocation(shader.ID, "intervalHours"),
        intervalHours
    );
    glUniform1f(
        glGetUniformLocation(shader.ID, "directRadiation"),
        directRadiation
    );
    glUniform1f(
        glGetUniformLocation(shader.ID, "diffuseRadiation"),
        diffuseRadiation
    );

    glBindVertexArray(VAO);

    const GLboolean previousCullFace = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_CULL_FACE);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    if (previousCullFace)
        glEnable(GL_CULL_FACE);

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
    glViewport(previousViewport[0], previousViewport[1],
               previousViewport[2], previousViewport[3]);

    // Restore previous blending configuration.
glBlendEquationSeparate(
    previousBlendEquationRGB,
    previousBlendEquationAlpha
);

glBlendFuncSeparate(
    previousBlendSrcRGB,
    previousBlendDstRGB,
    previousBlendSrcAlpha,
    previousBlendDstAlpha
);

// Restore previous enable states.
if (previousDepthTest)
    glEnable(GL_DEPTH_TEST);
else
    glDisable(GL_DEPTH_TEST);

if (previousBlend)
    glEnable(GL_BLEND);
else
    glDisable(GL_BLEND);
}

void HeatmapBaker::Draw(
    Shader& shader,
    const glm::mat4& view,
    const glm::mat4& projection
) const
{
    GLboolean previousDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean previousDepthMask;
    GLboolean previousCullFace = glIsEnabled(GL_CULL_FACE);

    GLint previousDepthFunc;

    glGetBooleanv(GL_DEPTH_WRITEMASK, &previousDepthMask);
    glGetIntegerv(GL_DEPTH_FUNC, &previousDepthFunc);

    shader.Use();
    shader.SetMat4("view", view);
    shader.SetMat4("projection", projection);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, heatmapTexture);

    glUniform1i(
        glGetUniformLocation(shader.ID, "heatmapTexture"), 0
    );

    glUniform1f(
        glGetUniformLocation(shader.ID, "maxRadiationWhM2"),
        3200.0f
    );

    // Enable depth testing and avoid writing to the depth buffer.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    glDisable(GL_CULL_FACE);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    // Restore OpenGL state.
    glDepthMask(previousDepthMask);
    glDepthFunc(previousDepthFunc);

    if (previousDepthTest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (previousCullFace)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
}
HeatmapStatistics HeatmapBaker::GetStatistics() const
{
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

    glBindTexture(GL_TEXTURE_2D, heatmapTexture);

    std::vector<float> values(RESOLUTION * RESOLUTION);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, values.data());

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

    HeatmapStatistics stats;
    stats.pixelCount = static_cast<int>(values.size());

    stats.minimum = std::numeric_limits<float>::max();
    stats.maximum = std::numeric_limits<float>::lowest();

    int validCount = 0;

    for (float value : values)
    {
        if (!std::isfinite(value))
            continue;

        stats.minimum = std::min(stats.minimum, value);
        stats.maximum = std::max(stats.maximum, value);
        stats.total += value;
        ++validCount;
    }

    if (validCount == 0)
    {
        stats.minimum = 0.0f;
        stats.maximum = 0.0f;
        stats.average = 0.0;
    }
    else
    {
        stats.average = stats.total / static_cast<double>(validCount);
    }

    return stats;
}

void HeatmapBaker::PrintStatistics() const
{
    const HeatmapStatistics stats = GetStatistics();

    std::cout
        << "\n========== HEAT-MAP STATISTICS =========="
        << "\nMinimum sunlight: " << stats.minimum
        << "\nMaximum sunlight: " << stats.maximum
        << "\nAverage sunlight: " << stats.average
        << "\nTotal pixels: " << stats.pixelCount
        << "\n========================================="
        << std::endl;
}

HeatmapBaker::~HeatmapBaker()
{
    if (EBO) glDeleteBuffers(1, &EBO);
    if (VBO) glDeleteBuffers(1, &VBO);
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (heatmapTexture) glDeleteTextures(1, &heatmapTexture);
    if (framebuffer) glDeleteFramebuffers(1, &framebuffer);
}
