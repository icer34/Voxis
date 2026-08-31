#pragma once

#include "mesh.h"
#include "game/chunk.h"
#include "game/block_registry.h"
#include "block_atlas.h"

namespace ChunkMesher
{
MeshData getMeshData(Chunk& chunk, const BlockRegistry& registry, const BlockAtlas& atlas);
} // namespace ChunkMesher
