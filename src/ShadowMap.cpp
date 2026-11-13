
#include "ShadowMap.hpp"

#include <iostream>
#include <stdexcept>

ShadowMap::ShadowMap()
{
    Initialize();
}

void ShadowMap::Initialize()
{
    // Create framebuffer.
    glGenFramebuffers(1, &FBO);

    // Create depth texture.
    glGenTextures(1, &depthTexture);

    glBindTexture(
        GL_TEXTURE_2D,
        depthTexture
    );

    // Allocate depth texture storage.
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_DEPTH_COMPONENT24,
        WIDTH,
        HEIGHT,
        0,
        GL_DEPTH_COMPONENT,
        GL_UNSIGNED_INT,
        nullptr
    );

    // Nearest filtering is appropriate because
    // PCF will perform our own neighbouring samples.
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_NEAREST
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_NEAREST
    );

    // Clamp outside the shadow map to lit depth.
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_BORDER
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_BORDER
    );

    float borderColor[] =
    {
        1.0f, 1.0f, 1.0f, 1.0f
    };

    glTexParameterfv(
        GL_TEXTURE_2D,
        GL_TEXTURE_BORDER_COLOR,
        borderColor
    );

    // Attach depth texture to framebuffer.
    glBindFramebuffer(
        GL_FRAMEBUFFER,
        FBO
    );

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_TEXTURE_2D,
        depthTexture,
        0
    );

    // We only need depth, not colour.
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Check framebuffer completeness.
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER)
        != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        throw std::runtime_error(
            "Shadow framebuffer is incomplete"
        );
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout
        << "Shadow map initialized: "
        << WIDTH << " x " << HEIGHT
        << std::endl;
}

void ShadowMap::BindForWriting() const
{
    glBindFramebuffer(
        GL_FRAMEBUFFER,
        FBO
    );

    glViewport(
        0,
        0,
        WIDTH,
        HEIGHT
    );

    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::Unbind() const
{
    glBindFramebuffer(
        GL_FRAMEBUFFER,
        0
    );
}

void ShadowMap::BindForReading(
    unsigned int textureUnit
) const
{
    glActiveTexture(GL_TEXTURE0 + textureUnit);

    glBindTexture(
        GL_TEXTURE_2D,
        depthTexture
    );
}

unsigned int ShadowMap::GetDepthTexture() const
{
    return depthTexture;
}

ShadowMap::~ShadowMap()
{
    if (depthTexture)
        glDeleteTextures(1, &depthTexture);

    if (FBO)
        glDeleteFramebuffers(1, &FBO);
}
