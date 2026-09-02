#pragma once

#include <cstdint>

class Chunk;
class BlockRegistry;

class TerrainGenerator
{
public:
    TerrainGenerator(uint64_t seed, BlockRegistry& registry);

    void generate(Chunk& chunk);

private:
    uint64_t _seed;
    BlockRegistry& _registry;
};