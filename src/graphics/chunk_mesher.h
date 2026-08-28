#pragma once

#include "mesh.h"
#include "game/chunk.h"

namespace ChunkMesher
{
MeshData getMeshData(Chunk& chunk);
} // namespace ChunkMesher