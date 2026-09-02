#pragma once

#include "util/handle.h"

#include <vector>

class Chunk;
class BlockRegistry;
class BlockAtlas;
class Renderer;

/**
 * @brief Builds the per-chunk 3D texture that stores, for every voxel, the block's already-resolved
 * atlas tile index - sampled by the fragment shader via texelFetch (see Phase 1.5 in todo.md)
 *
 */
namespace ChunkDataTexture
{
std::vector<uint16_t> build(Chunk& chunk, const BlockRegistry& registry, const BlockAtlas& atlas);
Texture3DHandle upload(const std::vector<uint16_t>& texTileIdx, Renderer& renderer);
} // namespace ChunkDataTexture
