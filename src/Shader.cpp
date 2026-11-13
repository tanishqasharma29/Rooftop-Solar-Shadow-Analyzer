
#include "Shader.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <glm/gtc/type_ptr.hpp>

Shader::Shader(
    const std::string& vertexPath,
    const std::string& fragmentPath
)
{
    std::string vertexSource = ReadFile(vertexPath);
    std::string fragmentSource = ReadFile(fragmentPath);

    unsigned int vertex = CompileShader(
        GL_VERTEX_SHADER,
        vertexSource
    );

    unsigned int fragment = CompileShader(
        GL_FRAGMENT_SHADER,
        fragmentSource
    );

    ID = glCreateProgram();

    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);

    glLinkProgram(ID);

    int success;
    char infoLog[1024];

    glGetProgramiv(
        ID,
        GL_LINK_STATUS,
        &success
    );

    if (!success)
    {
        glGetProgramInfoLog(
            ID,
            sizeof(infoLog),
            nullptr,
            infoLog
        );

        std::cerr << "Shader linking failed:\n"
                  << infoLog << std::endl;

        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(ID);

        throw std::runtime_error("Shader linking failed");
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    std::cout << "Lighting shader loaded successfully!\n";
}

std::string Shader::ReadFile(
    const std::string& path
)
{
    std::ifstream file(path);

    if (!file.is_open())
    {
        throw std::runtime_error(
            "Cannot open shader file: " + path
        );
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

unsigned int Shader::CompileShader(
    GLenum type,
    const std::string& source
)
{
    unsigned int shader = glCreateShader(type);

    const char* sourcePointer = source.c_str();

    glShaderSource(
        shader,
        1,
        &sourcePointer,
        nullptr
    );

    glCompileShader(shader);

    int success;
    char infoLog[1024];

    glGetShaderiv(
        shader,
        GL_COMPILE_STATUS,
        &success
    );

    if (!success)
    {
        glGetShaderInfoLog(
            shader,
            sizeof(infoLog),
            nullptr,
            infoLog
        );

        std::cerr << "Shader compilation failed:\n"
                  << infoLog << std::endl;

        glDeleteShader(shader);

        throw std::runtime_error("Shader compilation failed");
    }

    return shader;
}

void Shader::Use() const
{
    glUseProgram(ID);
}

void Shader::SetMat4(
    const std::string& name,
    const glm::mat4& value
) const
{
    GLint location = glGetUniformLocation(
        ID,
        name.c_str()
    );

    glUniformMatrix4fv(
        location,
        1,
        GL_FALSE,
        glm::value_ptr(value)
    );
}

void Shader::SetVec3(
    const std::string& name,
    const glm::vec3& value
) const
{
    GLint location = glGetUniformLocation(
        ID,
        name.c_str()
    );

    glUniform3fv(
        location,
        1,
        glm::value_ptr(value)
    );
}

void Shader::SetFloat(
    const std::string& name,
    float value
) const
{
    GLint location = glGetUniformLocation(
        ID,
        name.c_str()
    );

    glUniform1f(location, value);
}
