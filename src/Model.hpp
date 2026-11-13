
#pragma once

#include <string>
#include <vector>

#include <assimp/scene.h>

#include "Mesh.hpp"

class Model
{
public:

    explicit Model(const std::string& path);

    void Draw() const;

private:

    std::vector<Mesh> meshes;

    void LoadModel(const std::string& path);

    void ProcessNode(
        aiNode* node,
        const aiScene* scene
    );

    Mesh ProcessMesh(aiMesh* mesh);
};
