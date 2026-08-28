#pragma once

#include <vulkan/vulkan.h>

struct VmaAllocator_T;
typedef struct VmaAllocator_T* VmaAllocator;
struct VmaAllocation_T;
typedef struct VmaAllocation_T* VmaAllocation;

class Texture
{
public:
    Texture(VkImage image,
            VmaAllocation alloc,
            uint32_t width,
            uint32_t height,
            uint32_t mipLevels,
            VkFormat format,
            VkDevice device,
            VmaAllocator allocator);
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    ~Texture();

    VkDescriptorImageInfo descriptorInfo() const;

private:
    VkDevice _device;
    VmaAllocation _alloc;
    VmaAllocator _allocator;
    uint32_t _width = 0;
    uint32_t _height = 0;
    uint32_t _mipLevels = 0;
    VkFormat _format;
    VkImage _image;
    VkImageView _imageView;
    VkSampler _sampler;
};