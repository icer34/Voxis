#pragma once

#include "handle.h"

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
Texture3DHandle build(Chunk& chunk, const BlockRegistry& registry, const BlockAtlas& atlas, Renderer& renderer);
} // namespace ChunkDataTexture
