
#pragma once

#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
};

class Mesh
{
public: 
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    Mesh(
        const std::vector<Vertex>& vertices,
        const std::vector<unsigned int>& indices
    );

    ~Mesh();

    void Draw() const;

private:

    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;

    void SetupMesh();
};
