#pragma once

#include "vk_common.h"

/**
 * @brief GPU-side texture: owns the Vulkan image, image view and sampler, allocated via VMA
 *
 */
class Texture
{
public:
    Texture(VkImage image,
            VmaAllocation alloc,
            uint32_t width,
            uint32_t height,
            uint32_t mipLevels,
            VkFormat format,
            VkImageViewType viewType,
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