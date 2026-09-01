#pragma once

#include <vector>
#include <span>

#include "vk_common.h"
#include <glm/glm.hpp>

/**
 * @brief A single mesh vertex, packed into two uint32_t to keep chunk meshes small.
 *
 * A vertex never stores the absolute position of a quad corner (which can reach SIZE, one past the
 * last valid block index, and wouldn't fit in the bits below) - it stores the quad's base corner
 * (always a valid block index, 0..SIZE-1) plus its width/height, and the vertex shader reconstructs
 * the actual corner as base + (cornerID selects 0 or width/height along each axis). The same
 * width/height-based offset doubles as the local UV, so no separate UV field is needed.
 *
 * data1: baseX (4 bits) | baseY (4) | baseZ (4) | direction (3, one of 6 faces) | cornerID (2, which
 *        of the quad's 4 corners this vertex is) | AO (2, ambient occlusion level 0-3 for this
 *        corner) | unused (13 bits)
 *
 * data2: width-1 (4 bits) | height-1 (4 bits) | unused (24 bits, room for tint later)
 *        (stored as value-1, we map [1..SIZE] to [0..SIZE-1] to fit in the fewest bits possible)
 *
 */
struct Vertex
{
    uint32_t data1;
    uint32_t data2;
};

/**
 * @brief CPU-side vertex and index data for a mesh, before it is uploaded to the GPU
 *
 */
struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

/**
 * @brief GPU-side vertex and index buffers for a renderable mesh, allocated via VMA
 *
 */
class Mesh
{
public:
    Mesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices, VmaAllocator vmaAllocator);
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    ~Mesh();

    VkBuffer vertBuffer() const;
    VkBuffer idxBuffer() const;
    uint32_t idxCount() const;

private:
    VmaAllocator _allocator = nullptr;

    VkBuffer _vertBuff = nullptr;
    VmaAllocation _vertAlloc = nullptr;

    VkBuffer _idxBuff = nullptr;
    VmaAllocation _idxAlloc = nullptr;

    uint32_t _numIdx = 0;
};