
#pragma once

#include <glad/gl.h>

class ShadowMap
{
public:

    static constexpr unsigned int WIDTH = 2048;
    static constexpr unsigned int HEIGHT = 2048;

    ShadowMap();

    ~ShadowMap();

    // Begin rendering the scene from the Sun's perspective.
    void BindForWriting() const;

    // Restore the normal screen framebuffer.
    void Unbind() const;

    // Bind the depth texture for the lighting shader.
    void BindForReading(unsigned int textureUnit) const;

    unsigned int GetDepthTexture() const;

private:

    unsigned int FBO = 0;
    unsigned int depthTexture = 0;

    void Initialize();
};
