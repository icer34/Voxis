#include "chunk.h"

Chunk::Chunk(glm::ivec3 coords)
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
    _dirty = true;
}

uint16_t Chunk::getBlock(glm::ivec3 lCoords) const
{
    return _blockIDs[idx(lCoords)];
}

void Chunk::setMeshHandle(MeshHandle handle)
{
    _meshHandle = handle;
}

void Chunk::setDataTextureHandle(Texture3DHandle handle)
{
    _dataTextureHandle = handle;
}

MeshHandle Chunk::meshHandle() const
{
    return _meshHandle.value();
}

Texture3DHandle Chunk::dataTextureHandle() const
{
    return _dataTextureHandle.value();
}

bool Chunk::hasMesh() const
{
    return _meshHandle.has_value();
}

bool Chunk::hasDataTexture() const
{
    return _dataTextureHandle.has_value();
}

bool Chunk::isDirty() const
{
    return _dirty;
}

void Chunk::clearDirty()
{
    _dirty = false;
}

glm::ivec3 Chunk::coords() const
{
    return _coords;
}