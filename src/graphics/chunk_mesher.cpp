#include "chunk_mesher.h"

#include "directions.h"

#include <vector>

namespace
{
bool isSolid(Chunk& chunk, int x, int y, int z)
{
    constexpr int SIZE = static_cast<int>(Chunk::SIZE);
    if (x < 0 || y < 0 || z < 0 || x >= SIZE || y >= SIZE || z >= SIZE)
        return false;

    return chunk.getBlock({ x, y, z }) != 0;
}

void addFace(MeshData& mesh, glm::ivec3 basePos, Direction dir, uint32_t texTileIndex)
{
    uint32_t base = static_cast<uint32_t>(mesh.vertices.size());

    for (uint32_t cornerID = 0; cornerID < 4; cornerID++)
    {
        uint32_t data1 = 0;
        data1 |= (static_cast<uint32_t>(basePos.x) & 0xFu);
        data1 |= ((static_cast<uint32_t>(basePos.y) & 0xFu) << 4);
        data1 |= ((static_cast<uint32_t>(basePos.z) & 0xFu) << 8);
        data1 |= ((static_cast<uint32_t>(dir) & 0x7u) << 12);
        data1 |= ((cornerID & 0x3u) << 15);
        data1 |= ((texTileIndex & 0x7FFF) << 17);

        uint32_t data2 = 0; // all the fields of data2 are 0 for the naive (non-greedy) mesher

        mesh.vertices.push_back({ data1, data2 });
    }

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
MeshData getMeshData(Chunk& chunk, const BlockRegistry& registry, const BlockAtlas& atlas)
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

                glm::ivec3 p{ x, y, z };

                uint16_t blockID = chunk.getBlock(p);
                const std::string& blockName = registry.nameFromIdx(blockID);
                uint32_t texTileIndex = atlas.tileIndex(blockName);

                for (Direction dir : ALL_DIRECTIONS)
                {
                    glm::ivec3 neighborPos = p + DIRECTION_NORMALS[static_cast<uint32_t>(dir)];
                    if (isSolid(chunk, neighborPos.x, neighborPos.y, neighborPos.z))
                        continue;

                    addFace(mesh, p, dir, texTileIndex);
                }
            }
        }
    }

    return mesh;
}
} // namespace ChunkMesher