#include "chunk.h"

Chunk::Chunk(glm::ivec2 coords)
{
    _coords = coords;

    // fill the chunk with empty blocks by default
    for (size_t x = 0; x < SIZE; x++)
    {
        for (size_t y = 0; y < SIZE; y++)
        {
            for (size_t z = 0; z < SIZE; z++)
            {
                _blockIDs[idx(x, y, z)] = 0;
            }
        }
    }
}

Chunk::~Chunk() {}

void Chunk::setBlock(glm::ivec3 lCoords, uint16_t blockID)
{
    _blockIDs[idx(lCoords)] = blockID;
}

uint16_t Chunk::getBlock(glm::ivec3 lCoords)
{
    return _blockIDs[idx(lCoords)];
}

void Chunk::setMeshHandle(uint32_t handle)
{
    _meshHandle = handle;
}
