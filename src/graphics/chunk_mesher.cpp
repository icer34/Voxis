#include "chunk_mesher.h"

namespace
{
bool isSolid(Chunk& chunk, int x, int y, int z)
{
    constexpr int SIZE = static_cast<int>(Chunk::SIZE);
    if (x < 0 || y < 0 || z < 0 || x >= SIZE || y >= SIZE || z >= SIZE)
        return false;

    return chunk.getBlock({ x, y, z }) != 0;
}

void addFace(MeshData& mesh, glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 normal)
{
    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

    mesh.vertices.push_back({ v0, normal });
    mesh.vertices.push_back({ v1, normal });
    mesh.vertices.push_back({ v2, normal });
    mesh.vertices.push_back({ v3, normal });

    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
}
} // namespace

namespace ChunkMesher
{
MeshData getMeshData(Chunk& chunk)
{
    MeshData mesh;

    constexpr int SIZE = static_cast<int>(Chunk::SIZE);
    for (int x = 0; x < SIZE; x++)
    {
        for (int y = 0; y < SIZE; y++)
        {
            for (int z = 0; z < SIZE; z++)
            {
                if (!isSolid(chunk, x, y, z))
                    continue;

                glm::vec3 p(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));

                if (!isSolid(chunk, x + 1, y, z))
                    addFace(mesh,
                            p + glm::vec3(1, 0, 0),
                            p + glm::vec3(1, 1, 0),
                            p + glm::vec3(1, 1, 1),
                            p + glm::vec3(1, 0, 1),
                            glm::vec3(1, 0, 0));

                if (!isSolid(chunk, x - 1, y, z))
                    addFace(mesh,
                            p + glm::vec3(0, 0, 1),
                            p + glm::vec3(0, 1, 1),
                            p + glm::vec3(0, 1, 0),
                            p + glm::vec3(0, 0, 0),
                            glm::vec3(-1, 0, 0));

                if (!isSolid(chunk, x, y + 1, z))
                    addFace(mesh,
                            p + glm::vec3(0, 1, 0),
                            p + glm::vec3(0, 1, 1),
                            p + glm::vec3(1, 1, 1),
                            p + glm::vec3(1, 1, 0),
                            glm::vec3(0, 1, 0));

                if (!isSolid(chunk, x, y - 1, z))
                    addFace(mesh,
                            p + glm::vec3(0, 0, 1),
                            p + glm::vec3(0, 0, 0),
                            p + glm::vec3(1, 0, 0),
                            p + glm::vec3(1, 0, 1),
                            glm::vec3(0, -1, 0));

                if (!isSolid(chunk, x, y, z + 1))
                    addFace(mesh,
                            p + glm::vec3(1, 0, 1),
                            p + glm::vec3(1, 1, 1),
                            p + glm::vec3(0, 1, 1),
                            p + glm::vec3(0, 0, 1),
                            glm::vec3(0, 0, 1));

                if (!isSolid(chunk, x, y, z - 1))
                    addFace(mesh,
                            p + glm::vec3(0, 0, 0),
                            p + glm::vec3(0, 1, 0),
                            p + glm::vec3(1, 1, 0),
                            p + glm::vec3(1, 0, 0),
                            glm::vec3(0, 0, -1));
            }
        }
    }

    return mesh;
}
} // namespace ChunkMesher
