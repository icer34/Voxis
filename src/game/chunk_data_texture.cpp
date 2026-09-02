#include "chunk_data_texture.h"

#include <vector>

#include "chunk.h"
#include "block_registry.h"
#include "graphics/block_atlas.h"
#include "graphics/vulkan/renderer.h"

namespace ChunkDataTexture
{
std::vector<uint16_t> build(Chunk& chunk, const BlockRegistry& registry, const BlockAtlas& atlas)
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

    return tileIndices;
}

Texture3DHandle upload(const std::vector<uint16_t>& texTileIdx, Renderer& renderer)
{
    const unsigned char* mip0 = reinterpret_cast<const unsigned char*>(texTileIdx.data());
    return renderer.createTexture3D({ Chunk::SIZE, Chunk::SIZE, Chunk::SIZE },
                                    1,
                                    VK_FORMAT_R16_UINT,
                                    sizeof(uint16_t),
                                    &mip0);
}
} // namespace ChunkDataTexture
