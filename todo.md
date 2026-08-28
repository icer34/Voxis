## Chunk Meshing:

# 1 - representation
    represent the blockIDs in binary (0 if the block is empty, 1 if it's solid)

# 2 
    seperate the work in 6, for each direction

# 3 - for each direction
    - find which faces should be rendered (columns)
    - swizzle the data, apply greedy meshing

# 4
    - merge all the data into a single MeshData

## Textures:

# DONE - handles (Handle<Tag> template in renderer.h, MeshHandle/TextureHandle, std::hash specialization)
# DONE - Renderer::createTexture (staging buffer, VkImage, upload barriers, copy, construct in _textures)

# 1 - load pixels (CPU side, no Vulkan)
    - add stb_image.h as a dependency (single header, not present yet)
    - load the atlas PNG into a raw RGBA byte buffer + width/height
    - mips are pre-generated at atlas tiling time (own tool/step), so load all mipLevels images
      (one per level, each half the size of the previous, down to 1x1) instead of just level 0
      - this avoids the cross-tile bleeding a naive whole-atlas blit would cause, since each mip is
        generated with per-tile knowledge at packing time

# 2 - block -> tile association (game side, no Vulkan)
    - table: blockID (+ face) -> tile index in the atlas grid
    - in ChunkMesher, when emitting a face, compute the UV rect from the tile index
    - Vertex gains a uv field (or packed into the same uint32 as position/normal if going the bit-packed route)

# 3 - texture.cpp: implement the Texture class (currently declared in texture.h, file doesn't exist yet)
    (this is what's currently breaking the link: undefined reference to Texture::Texture/~Texture)

    ctor(VkImage image, VmaAllocation alloc, uint32_t width, uint32_t height, uint32_t mipLevels,
         VkFormat format, VkDevice device, VmaAllocator allocator)
    - store _image, _allocation, _device, _allocator as members (texture.h's private: section is currently empty,
      add these + _imageView + _sampler)
    - VkImageViewCreateInfo viewInfo{};
      viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      viewInfo.image = image;
      viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      viewInfo.format = format;
      viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 };  // levelCount = mipLevels, not 1
      vkCreateImageView(device, &viewInfo, nullptr, &_imageView);
    - VkSamplerCreateInfo samplerInfo{};
      samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
      samplerInfo.magFilter = VK_FILTER_NEAREST;   // blocky voxel look
      samplerInfo.minFilter = VK_FILTER_NEAREST;
      samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;  // avoid wrap bleeding into other atlas tiles
      samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
      samplerInfo.anisotropyEnable = VK_FALSE;
      samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
      samplerInfo.minLod = 0.0f;
      samplerInfo.maxLod = static_cast<float>(mipLevels);
      samplerInfo.mipLodBias = 0.0f;
      vkCreateSampler(device, &samplerInfo, nullptr, &_sampler);

    dtor:
      vkDestroySampler(_device, _sampler, nullptr);
      vkDestroyImageView(_device, _imageView, nullptr);
      vmaDestroyImage(_allocator, _image, _allocation);   // vmaDestroyIMAGE, not DestroyBuffer

    descriptorInfo() const:
      return VkDescriptorImageInfo{ _sampler, _imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

    also: Renderer::destroyTexture(handle) is still an empty {} body -> just needs `_textures.erase(handle);`
    (same one-liner as destroyMesh)

# 4 - descriptor set plumbing for the texture (once, separate from the per-frame camera set)
    new members on Renderer: VkDescriptorSetLayout _textureSetLayout, VkDescriptorSet _textureDescriptorSet

    in createGrpahicsPipeline(), next to the existing camera set layout:
      VkDescriptorSetLayoutBinding texBinding{};
      texBinding.binding = 0;
      texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      texBinding.descriptorCount = 1;
      texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

      VkDescriptorSetLayoutCreateInfo texSetLayoutInfo{};
      texSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      texSetLayoutInfo.bindingCount = 1;
      texSetLayoutInfo.pBindings = &texBinding;
      vkCreateDescriptorSetLayout(_device, &texSetLayoutInfo, nullptr, &_textureSetLayout);

    pipeline layout: setLayoutCount = 2, pSetLayouts = { _descriptorSetLayout, _textureSetLayout }
      (order matters: index 0 = set 0 = camera, index 1 = set 1 = texture, must match the shader's layout(set=N))

    _descriptorPool: add a second VkDescriptorPoolSize { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }
      (only 1 needed - one texture set total, not one per frame in flight), bump maxSets from
      MAX_FRAMES_IN_FLIGHT to MAX_FRAMES_IN_FLIGHT + 1

    allocate _textureDescriptorSet once from that pool (VkDescriptorSetAllocateInfo, descriptorSetCount = 1,
      pSetLayouts = &_textureSetLayout)

    the actual vkUpdateDescriptorSets can only happen once a real Texture exists - suggest a small dedicated
    method, e.g. Renderer::setBlockAtlas(TextureHandle handle):
      - look up the Texture in _textures
      - VkWriteDescriptorSet write{}; write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = _textureDescriptorSet; write.dstBinding = 0; write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        VkDescriptorImageInfo imgInfo = texture.descriptorInfo();
        write.pImageInfo = &imgInfo;
        vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);
      - called once, right after the atlas texture is created - never touched again

# 5 - shader changes
    - vertex shader: pass UV through as a varying (layout(location = N) out vec2 outUV)
    - fragment shader: layout(set = 1, binding = 0) uniform sampler2D texAtlas; and sample with texture(texAtlas, inUV)

# 6 - per-frame binding (in recordFrame, every frame)
    vkCmdBindDescriptorSets can bind multiple CONSECUTIVE sets in one call - since camera=set0 and
    texture=set1 are consecutive, replace the current single-set call with:

      std::array<VkDescriptorSet, 2> sets{ frame.uniformDescriptorSet, _textureDescriptorSet };
      vkCmdBindDescriptorSets(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout,
                              0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

    cheap (just a recorded command), even though nothing about the texture ever changes frame to frame
