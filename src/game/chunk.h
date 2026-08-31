#pragma once

#include <array>

#include <glm/glm.hpp>

/**
 * @brief A fixed-size cube of blocks that makes up a portion of the world
 *
 */
class Chunk
{
public:
    static constexpr uint32_t SIZE{ 16 };

    Chunk(glm::ivec2 coords);
    ~Chunk();

    void setMeshHandle(uint32_t handle);

    void setBlock(glm::ivec3 lCoords, uint16_t blockID);
    uint16_t getBlock(glm::ivec3 lCoords) const;

    glm::ivec2 coords() const;

private:
    glm::ivec2 _coords;

    std::array<uint16_t, SIZE * SIZE * SIZE> _blockIDs;
    static constexpr size_t idx(size_t x, size_t y, size_t z) { return x + y * SIZE + z * SIZE * SIZE; }
    static constexpr size_t idx(glm::ivec3 coords)
    {
        return static_cast<uint32_t>(coords.x) + static_cast<uint32_t>(coords.y) * SIZE +
               static_cast<uint32_t>(coords.z) * SIZE * SIZE;
    }

    uint32_t _meshHandle = 0;
};