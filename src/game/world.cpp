#include "world.h"

#include <cmath>

#include "graphics/chunk_data_texture.h"
#include "graphics/chunk_mesher.h"
#include "graphics/renderer.h"
#include "directions.h"
#include "log.h"

World::World(const uint64_t seed, BlockRegistry& registry, BlockAtlas& atlas, Renderer& renderer)
    : _seed(seed),
      _terrainGenerator(_seed, registry),
      _renderer(renderer),
      _atlas(atlas),
      _registry(registry)
{
}

World::~World() {}

void World::update(double dt, glm::vec3 playerPos)
{
    glm::ivec3 playerChunk = glm::ivec3(glm::floor(playerPos / static_cast<float>(Chunk::SIZE)));

    // unload all the chunks that are not in the player's render distance
    std::vector<glm::ivec3> toUnload;
    for (const auto& [coords, chunk] : _chunks)
    {
        glm::ivec3 delta = coords - playerChunk;
        if (std::abs(delta.x) > RENDER_DISTANCE || std::abs(delta.y) > RENDER_DISTANCE ||
            std::abs(delta.z) > RENDER_DISTANCE)
            toUnload.push_back(coords);
    }

    for (glm::ivec3 coords : toUnload)
    {
        Chunk& chunk = _chunks.at(coords);
        if (chunk.hasDataTexture())
            _renderer.destroyTexture3D(chunk.dataTextureHandle());
        if (chunk.hasMesh())
            _renderer.destroyMesh(chunk.meshHandle());
        _chunks.erase(coords);
    }

    // generate the chunks - the budget check is repeated in every loop level so reaching it
    // actually stops the whole sweep, not just the innermost z loop
    uint32_t generatedChunks = 0;
    for (int r = 0; r < RENDER_DISTANCE && generatedChunks < MAX_CHUNK_LOADS_PER_TICK; r++)
    {
        for (int x = -r; x <= r && generatedChunks < MAX_CHUNK_LOADS_PER_TICK; x++)
        {
            for (int y = -r; y <= r && generatedChunks < MAX_CHUNK_LOADS_PER_TICK; y++)
            {
                for (int z = -r; z <= r && generatedChunks < MAX_CHUNK_LOADS_PER_TICK; z++)
                {
                    if (std::abs(x) != r && std::abs(y) != r && std::abs(z) != r)
                        continue;

                    glm::ivec3 chunkCoord = playerChunk + glm::ivec3(x, y, z);
                    if (_chunks.contains(chunkCoord))
                        continue;

                    scheduleChunkGeneration(chunkCoord);

                    generatedChunks++;
                }
            }
        }
    }

    // generate the meshes
    uint32_t meshedChunks = 0;
    for (auto& [coords, chunk] : _chunks)
    {
        if (meshedChunks > MAX_CHUNK_LOADS_PER_TICK)
            break;

        // continue if the chunk is already meshed
        if (chunk.state() == ChunkState::MESHED || chunk.state() == ChunkState::NOT_GENERATED)
            continue;

        // check all neighbors, if only one is not generated -> continue
        std::array<const Chunk*, 6> neighbors{};
        bool shouldMesh = true;
        for (Direction dir : ALL_DIRECTIONS)
        {
            glm::ivec3 neighborCoords = coords + DIRECTION_NORMALS[static_cast<size_t>(dir)];
            auto it = _chunks.find(neighborCoords);
            if (it == _chunks.end() || it->second.state() == ChunkState::NOT_GENERATED)
            {
                shouldMesh = false;
                break;
            }

            neighbors[static_cast<size_t>(dir)] = &it->second;
        }

        if (!shouldMesh)
            continue;

        scheduleChunkMeshing(chunk, neighbors);
        meshedChunks++;
    }
}

void World::scheduleChunkGeneration(glm::ivec3 coords)
{
    // place a dummy chunk in the _chunks map
    auto [it, result] = _chunks.try_emplace(coords, coords);
    if (!result)
        VoxisLog::critical("Failed to insert chunk, coords = [{}, {}, {}]", coords.x, coords.y, coords.z);

    // start a worker job to generate the actual chunk data (sequential for now for testing purposes)
    _terrainGenerator.generate(it->second);
    it->second.setState(ChunkState::GENERATED);
}

void World::scheduleChunkMeshing(Chunk& chunk, std::array<const Chunk*, 6> neighbors)
{
    if (chunk.hasMesh())
        _renderer.destroyMesh(chunk.meshHandle());
    if (chunk.hasDataTexture())
        _renderer.destroyTexture3D(chunk.dataTextureHandle());

    MeshData mesh = ChunkMesher::getMeshData(chunk, neighbors);

    // a chunk fully enclosed by solid neighbors has no visible faces at all - don't create a GPU
    // mesh/texture for it (a 0-vertex mesh is an invalid Vulkan buffer size), it just won't render
    if (!mesh.vertices.empty())
    {
        chunk.setMeshHandle(_renderer.createMesh(mesh.vertices, mesh.indices));
        chunk.setDataTextureHandle(ChunkDataTexture::build(chunk, _registry, _atlas, _renderer));
    }

    chunk.setState(ChunkState::MESHED);
}

std::vector<const Chunk*> World::getRenderableChunks() const
{
    std::vector<const Chunk*> chunks;
    chunks.reserve(_chunks.size());

    for (const auto& [coords, chunk] : _chunks)
    {
        if (chunk.hasDataTexture() && chunk.hasMesh())
            chunks.push_back(&chunk);
    }

    return chunks;
}
