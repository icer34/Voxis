#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <unordered_map>
#include <cstdint>

#include "terrain_generator.h"

class BlockAtlas;
class Renderer;
class BlockRegistry;
class Chunk;

namespace std
{
// define a hashing function for an integer vec3 (used for chunk coords)
template <> struct hash<glm::ivec3>
{
    size_t operator()(const glm::ivec3& v) const noexcept
    {
        size_t seed = std::hash<int>{}(v.x);
        seed ^= std::hash<int>{}(v.y) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(v.z) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};
} // namespace std

/**
 * @brief Holds and manages the chunks that make up the game world
 *
 */
class World
{
public:
    World(const uint64_t seed, BlockRegistry& registry, BlockAtlas& atlas, Renderer& renderer);
    ~World();

    void update(double dt, glm::vec3 playerPos);

    std::vector<const Chunk*> getRenderableChunks() const;

private:
    constexpr static int RENDER_DISTANCE = 10;
    constexpr static uint32_t MAX_CHUNK_LOADS_PER_TICK = 50;

    uint64_t _seed;

    TerrainGenerator _terrainGenerator;
    Renderer& _renderer;
    BlockAtlas& _atlas;
    BlockRegistry& _registry;

    std::unordered_map<glm::ivec3, Chunk> _chunks;

    void scheduleChunkGeneration(glm::ivec3 coords);
    void scheduleChunkMeshing(Chunk& chunk, std::array<const Chunk*, 6> neighbors);
};