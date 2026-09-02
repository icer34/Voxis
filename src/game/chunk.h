#pragma once

#include <array>
#include <optional>

#include <glm/glm.hpp>

#include "util/handle.h"

/**
 * @brief Holds the chunk status, used for synchronization between generation / meshing
 *
 */
enum class ChunkState
{
    NOT_GENERATED, // default state, has just been created and its terrain is not generated
    GENERATED,     // chunk terrain is generated but not yet meshed
    MESHED,        // meshed, ready to be rendered
    DIRTY          // valid terrain but modified since last mesh --> needs remesh
};

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

    void setState(ChunkState state);
    ChunkState state() const;

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

    ChunkState _state = ChunkState::NOT_GENERATED;
};