#pragma once

#include <array>

#include "vulkan/mesh.h"
#include "game/chunk.h"
#include "game/block_registry.h"

namespace ChunkMesher
{
/**
 * @brief Builds the greedy-meshed geometry for a chunk
 *
 * @param chunk
 * @param neighbors the 6 chunks adjacent to `chunk` (indexed by Direction), used to see across
 * chunk boundaries instead of always treating them as air - none are expected to be nullptr
 */
MeshData getMeshData(Chunk& chunk, std::array<const Chunk*, 6> neighbors);
} // namespace ChunkMesher
