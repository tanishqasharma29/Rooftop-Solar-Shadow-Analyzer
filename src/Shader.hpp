
#pragma once

#include <string>

#include <glad/gl.h>
#include <glm/glm.hpp>

class Shader
{
public:

    unsigned int ID;

    Shader(
        const std::string& vertexPath,
        const std::string& fragmentPath
    );

    void Use() const;

    void SetMat4(
        const std::string& name,
        const glm::mat4& value
    ) const;

    void SetVec3(
        const std::string& name,
        const glm::vec3& value
    ) const;

    void SetFloat(
        const std::string& name,
        float value
    ) const;

private:

    std::string ReadFile(
        const std::string& path
    );

    unsigned int CompileShader(
        GLenum type,
        const std::string& source
    );
};
