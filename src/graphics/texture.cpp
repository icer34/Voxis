#include "texture.h"

#include <vk_mem_alloc.h>

#include "log.h"

Texture::Texture(VkImage image,
                 VmaAllocation alloc,
                 uint32_t width,
                 uint32_t height,
                 uint32_t mipLevels,
                 VkFormat format,
                 VkDevice device,
                 VmaAllocator allocator)
{
    _device = device;
    _image = image;
    _alloc = alloc;
    _allocator = allocator;
    _width = width;
    _height = height;
    _mipLevels = mipLevels;
    _format = format;

    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.image = _image;
    imageViewInfo.format = _format;
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, _mipLevels, 0, 1 };

    if (vkCreateImageView(_device, &imageViewInfo, nullptr, &_imageView) != VK_SUCCESS)
        VoxisLog::critical("Failed to create texture image view");

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);
    samplerInfo.mipLodBias = 0.0f;

    if (vkCreateSampler(_device, &samplerInfo, nullptr, &_sampler) != VK_SUCCESS)
        VoxisLog::critical("Failed to create texture sampler");
}

Texture::~Texture()
{
    vkDestroySampler(_device, _sampler, nullptr);
    vkDestroyImageView(_device, _imageView, nullptr);
    vmaDestroyImage(_allocator, _image, _alloc);
}

VkDescriptorImageInfo Texture::descriptorInfo() const
{
    return VkDescriptorImageInfo{ _sampler, _imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
}
