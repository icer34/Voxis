#include "chunk_data_texture.h"

#include <vector>

#include "game/chunk.h"
#include "game/block_registry.h"
#include "block_atlas.h"
#include "renderer.h"

namespace ChunkDataTexture
{
Texture3DHandle build(Chunk& chunk, const BlockRegistry& registry, const BlockAtlas& atlas, Renderer& renderer)
{
    constexpr int SIZE = static_cast<int>(Chunk::SIZE);

    std::vector<uint16_t> tileIndices(static_cast<size_t>(SIZE) * SIZE * SIZE, 0);

    for (int x = 0; x < SIZE; x++)
    {
        for (int y = 0; y < SIZE; y++)
        {
            for (int z = 0; z < SIZE; z++)
            {
                uint16_t blockID = chunk.getBlock({ x, y, z });
                if (blockID == 0) // air, keep the default tile index of 0 (never sampled)
                    continue;

                size_t i = static_cast<size_t>(x) + static_cast<size_t>(y) * SIZE +
                           static_cast<size_t>(z) * SIZE * SIZE;
                tileIndices[i] = static_cast<uint16_t>(atlas.tileIndex(registry.nameFromIdx(blockID)));
            }
        }
    }

    const unsigned char* mip0 = reinterpret_cast<const unsigned char*>(tileIndices.data());
    return renderer.createTexture3D({ SIZE, SIZE, SIZE }, 1, VK_FORMAT_R16_UINT, sizeof(uint16_t), &mip0);
}
} // namespace ChunkDataTexture
