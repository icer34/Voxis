#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <unordered_map>
#include <cstdint>

#include "chunk.h"
#include "block_registry.h"

class BlockAtlas;
class Renderer;

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
    World(const uint64_t seed, const BlockRegistry& registry, const BlockAtlas& atlas, Renderer& renderer);
    ~World();

    void update(double dt);

    std::vector<const Chunk*> getChunks() const;

private:
    constexpr static uint32_t RENDER_DISTANCE = 10;

    // very basic placeholder terrain shape - a single sine wave along X, no actual noise yet
    constexpr static float TERRAIN_FREQUENCY = 0.1f;
    constexpr static float TERRAIN_AMPLITUDE = 4.0f;
    constexpr static float TERRAIN_BASE_HEIGHT = 8.0f;

    uint64_t _seed;
    const BlockRegistry& _registry;
    const BlockAtlas& _atlas;
    Renderer& _renderer;
    std::unordered_map<glm::ivec3, Chunk> _chunks;

    void generate();
    void generateTerrain(Chunk& chunk) const;
};