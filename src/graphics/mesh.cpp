#include "mesh.h"

#include <vk_mem_alloc.h>

#include "log.h"

Mesh::Mesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices, VmaAllocator vmaAllocator)
{
    _numIdx = static_cast<uint32_t>(indices.size());
    _allocator = vmaAllocator;

    //===== VERTEX BUFFER =====
    // allocate the buffer memory on the gpu
    VkBufferCreateInfo vertBuffInfo{};
    vertBuffInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertBuffInfo.size = vertices.size() * sizeof(Vertex);
    vertBuffInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo vertAllocCreateInfo{};
    vertAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    vertAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo vertAllocInfo{};

    if (vmaCreateBuffer(_allocator, &vertBuffInfo, &vertAllocCreateInfo, &_vertBuff, &_vertAlloc, &vertAllocInfo) !=
        VK_SUCCESS)
        VoxisLog::critical("Failed to create mesh vertex buffer");

    // cpy the data to the gpu buffer
    memcpy(vertAllocInfo.pMappedData, vertices.data(), vertices.size() * sizeof(Vertex));

    //===== INDEX BUFFER =====
    // allocate the buffer memory on the gpu
    VkBufferCreateInfo idxBuffInfo{};
    idxBuffInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    idxBuffInfo.size = indices.size() * sizeof(uint32_t);
    idxBuffInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    VmaAllocationCreateInfo idxAllocCreateInfo{};
    idxAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    idxAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo idxAllocInfo{};

    if (vmaCreateBuffer(_allocator, &idxBuffInfo, &idxAllocCreateInfo, &_idxBuff, &_idxAlloc, &idxAllocInfo) !=
        VK_SUCCESS)
        VoxisLog::critical("Failed to create mesh index buffer");

    // cpy the data to the gpu buffer
    memcpy(idxAllocInfo.pMappedData, indices.data(), indices.size() * sizeof(uint32_t));
}

Mesh::~Mesh()
{
    vmaDestroyBuffer(_allocator, _vertBuff, _vertAlloc);
    vmaDestroyBuffer(_allocator, _idxBuff, _idxAlloc);
}

VkBuffer Mesh::vertBuffer() const
{
    return _vertBuff;
}

VkBuffer Mesh::idxBuffer() const
{
    return _idxBuff;
}

uint32_t Mesh::idxCount() const
{
    return _numIdx;
}
