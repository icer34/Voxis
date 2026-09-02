#include "terrain_generator.h"

#include "chunk.h"
#include "block_registry.h"

TerrainGenerator::TerrainGenerator(uint64_t seed, BlockRegistry& registry)
    : _seed(seed),
      _registry(registry)
{
}

void TerrainGenerator::generate(Chunk& chunk)
{
    uint16_t stoneID = _registry.idxFromName("stone");

    for (uint32_t x = 0; x < Chunk::SIZE; x++)
    {
        for (uint32_t z = 0; z < Chunk::SIZE; z++)
        {
            for (uint32_t y = 0; y < Chunk::SIZE; y++)
            {
                glm::ivec3 worldPos = glm::ivec3(x, y, z) + static_cast<int>(Chunk::SIZE) * chunk.coords();

                if (worldPos.y > 0)
                    continue;

                chunk.setBlock({ x, y, z }, stoneID);
            }
        }
    }
}