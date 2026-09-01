#pragma once

#include "mesh.h"
#include "game/chunk.h"
#include "game/block_registry.h"

namespace ChunkMesher
{
MeshData getMeshData(Chunk& chunk, const BlockRegistry& registry);
} // namespace ChunkMesher
