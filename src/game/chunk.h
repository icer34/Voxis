#pragma once

#include <array>
#include <optional>

#include <glm/glm.hpp>

#include "graphics/handle.h"

/**
 * @brief A fixed-size cube of blocks that makes up a portion of the world
 *
 */
class Chunk
{
public:
    static constexpr uint32_t SIZE{ 16 };

    Chunk(glm::ivec3 coords);
    ~Chunk();

    void setMeshHandle(MeshHandle handle);
    void setDataTextureHandle(Texture3DHandle handle);

    MeshHandle meshHandle() const;
    Texture3DHandle dataTextureHandle() const;
    bool hasMesh() const;
    bool hasDataTexture() const;

    void setBlock(glm::ivec3 lCoords, uint16_t blockID);
    uint16_t getBlock(glm::ivec3 lCoords) const;

    /**
     * @brief Whether the chunk's blocks changed since its mesh/data texture were last (re)built -
     * both must be rebuilt together, since they're both derived from the same block data
     */
    bool isDirty() const;
    void clearDirty();

    glm::ivec3 coords() const;

private:
    glm::ivec3 _coords;

    std::array<uint16_t, SIZE * SIZE * SIZE> _blockIDs;
    static constexpr size_t idx(size_t x, size_t y, size_t z) { return x + y * SIZE + z * SIZE * SIZE; }
    static constexpr size_t idx(glm::ivec3 coords)
    {
        return static_cast<uint32_t>(coords.x) + static_cast<uint32_t>(coords.y) * SIZE +
               static_cast<uint32_t>(coords.z) * SIZE * SIZE;
    }

    std::optional<MeshHandle> _meshHandle;
    std::optional<Texture3DHandle> _dataTextureHandle;
    bool _dirty = true;
};