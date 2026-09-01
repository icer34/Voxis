#include "world.h"

#include <cmath>

#include "graphics/chunk_data_texture.h"
#include "graphics/chunk_mesher.h"
#include "graphics/renderer.h"
#include "graphics/block_atlas.h"

World::World(const uint64_t seed, const BlockRegistry& registry, const BlockAtlas& atlas, Renderer& renderer)
    : _seed(seed),
      _registry(registry),
      _atlas(atlas),
      _renderer(renderer)
{
    // for now generate just the initial chunks (no streaming, basic sine-wave terrain) to test
    // the greedy meshing on more than a single chunk
    generate();
}

World::~World() {}

void World::update(double dt) {}

std::vector<const Chunk*> World::getChunks() const
{
    std::vector<const Chunk*> chunks;
    chunks.reserve(_chunks.size());

    for (const auto& [key, value] : _chunks)
        chunks.push_back(&value);

    return chunks;
}

void World::generate()
{
    // generate a single chunk for now, for testing purpose

    glm::ivec3 coords{ 0, 0, 0 };
    auto [it, inserted] = _chunks.try_emplace(coords, coords);
    generateTerrain(it->second);

    MeshData mesh = ChunkMesher::getMeshData(it->second, _registry);
    it->second.setMeshHandle(_renderer.createMesh(mesh.vertices, mesh.indices));
    it->second.setDataTextureHandle(ChunkDataTexture::build(it->second, _registry, _atlas, _renderer));
}

void World::generateTerrain(Chunk& chunk) const
{
    uint16_t stoneID = _registry.idxFromName("stone");
    uint16_t dirtID = _registry.idxFromName("dirt");

    for (int x = 0; x < static_cast<int>(Chunk::SIZE); x++)
    {
        for (int z = 0; z < static_cast<int>(Chunk::SIZE); z++)
        {
            int worldZ = chunk.coords().z * static_cast<int>(Chunk::SIZE) + z;

            float heightF = TERRAIN_BASE_HEIGHT +
                            TERRAIN_AMPLITUDE * std::sin(static_cast<float>(worldZ) * TERRAIN_FREQUENCY);
            int height = static_cast<int>(heightF);

            for (int y = 0; y <= height && y < static_cast<int>(Chunk::SIZE); y++)
                (x + z) % 2 == 0 ? chunk.setBlock({ x, y, z }, stoneID) : chunk.setBlock({ x, y, z }, dirtID);
        }
    }
}
