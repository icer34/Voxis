#pragma once

#include <vector>
#include <span>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

/**
 * @brief A single mesh vertex: position and normal
 *
 */
struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
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

struct VmaAllocator_T;
typedef struct VmaAllocator_T* VmaAllocator;
struct VmaAllocation_T;
typedef struct VmaAllocation_T* VmaAllocation;

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