#include "renderer.h"

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <algorithm>

#include "plateform/window.h"
#include "camera.h"
#include "log.h"
#include "util.h"

Renderer::Renderer(Window& window)
    : _window(window)
{
    initVulkan();
}

Renderer::~Renderer()
{
    // destroy all meshes
    _meshes.clear();

    // cleanup all textures
    _textures.clear();

    shutdownVulkan();
}

void Renderer::render(Camera& cam, std::span<std::pair<MeshHandle, glm::mat4>> meshes)
{
    cam.setAspectRatio(static_cast<float>(_window.width()) / static_cast<float>(_window.height()));

    FrameResources& frame = _frameResources[_frameIndex % MAX_FRAMES_IN_FLIGHT];

    uint32_t imageIndex = 0;
    getNextImageIndex(frame, &imageIndex);

    // update uniforms
    struct CamUniforms
    {
        glm::mat4 view;
        glm::mat4 proj;
    };

    auto camUniforms = CamUniforms{ cam.getViewMatrix(), cam.getProjectionMatrix() };
    memcpy(frame.uniformsMapped, &camUniforms, sizeof(CamUniforms));

    recordFrame(frame, imageIndex, meshes);

    submitFrame(frame, imageIndex);

    presentFrame(frame, imageIndex);

    _frameIndex++;
}

MeshHandle Renderer::createMesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices)
{
    MeshHandle handle{ static_cast<uint32_t>(_meshes.size()) + 1 };
    auto result = _meshes.try_emplace(handle, vertices, indices, _vmaAllocator);
    if (!result.second)
        VoxisLog::critical("Failed to insert mesh with handle: {}", handle.value);

    return handle;
}

void Renderer::destroyMesh(MeshHandle handle)
{
    _meshes.erase(handle);
}

TextureHandle Renderer::createTexture(uint32_t width, uint32_t height, uint32_t mipLevels, const unsigned char** data)
{
    // get the size info for all the mip levels
    struct MipInfo
    {
        size_t offset;
        uint32_t width;
        uint32_t height;
    };
    std::vector<MipInfo> mips(mipLevels);
    size_t totalSize = 0;

    for (uint32_t i = 0; i < mipLevels; i++)
    {
        uint32_t levelWidth = std::max(1u, width >> i);
        uint32_t levelHeight = std::max(1u, height >> i);
        uint32_t levelSize = levelWidth * levelHeight * 4; // 4 channels

        mips[i] = { totalSize, levelWidth, levelHeight };
        totalSize += levelSize;
    }

    // create a staging buffer (used to transfer the texture data froim CPU to GPU)
    VkBufferCreateInfo buffInfo{};
    buffInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffInfo.size = totalSize * sizeof(char);
    buffInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocation buffAlloc{};
    VkBuffer buffer = nullptr;
    VmaAllocationInfo allocInfo{};
    if (vmaCreateBuffer(_vmaAllocator, &buffInfo, &allocCreateInfo, &buffer, &buffAlloc, &allocInfo) != VK_SUCCESS)
        VoxisLog::critical("Failed to create texture staging buffer");

    // copy the image data into the buffer
    for (uint32_t i = 0; i < mipLevels; i++)
    {
        uint32_t levelSize = mips[i].width * mips[i].height * 4;
        memcpy(static_cast<char*>(allocInfo.pMappedData) + mips[i].offset, data[i], levelSize);
    }

    // create the VkImage
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo imageAllocCreateInfo{};
    imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkImage image = nullptr;
    VmaAllocation imageAlloc = nullptr;

    if (vmaCreateImage(_vmaAllocator, &imageInfo, &imageAllocCreateInfo, &image, &imageAlloc, nullptr) != VK_SUCCESS)
        VoxisLog::critical("Failed to create the texture image");

    // record frame (send data from buffer to image)
    // 1. image data: undefined  --> transfer_dst_optimal
    VkCommandBuffer cmd = beginOneShotCommands();

    VkImageMemoryBarrier2 toDstBarrier{};
    toDstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toDstBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    toDstBarrier.srcAccessMask = 0;
    toDstBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toDstBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toDstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toDstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDstBarrier.image = image;
    toDstBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 };

    VkDependencyInfo toDstDependency{};
    toDstDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    toDstDependency.imageMemoryBarrierCount = 1;
    toDstDependency.pImageMemoryBarriers = &toDstBarrier;
    vkCmdPipelineBarrier2(cmd, &toDstDependency);

    // one copy region per mip level
    std::vector<VkBufferImageCopy> regions(mipLevels);
    for (uint32_t i = 0; i < mipLevels; i++)
    {
        regions[i].bufferOffset = mips[i].offset;
        regions[i].imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1 };
        regions[i].imageExtent = { mips[i].width, mips[i].height, 1 };
    }
    vkCmdCopyBufferToImage(cmd,
                           buffer,
                           image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<uint32_t>(regions.size()),
                           regions.data());

    // 2. image data: transfer_dst_optimal --> shader_read_only_optimal
    VkImageMemoryBarrier2 toShaderBarrier{};
    toShaderBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toShaderBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    toShaderBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    toShaderBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    toShaderBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    toShaderBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShaderBarrier.image = image;
    toShaderBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 1 };

    VkDependencyInfo toShaderDependency{};
    toShaderDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    toShaderDependency.imageMemoryBarrierCount = 1;
    toShaderDependency.pImageMemoryBarriers = &toShaderBarrier;
    vkCmdPipelineBarrier2(cmd, &toShaderDependency);

    endOneShotCommands(cmd);

    // destroy the now unused buffer
    vmaDestroyBuffer(_vmaAllocator, buffer, buffAlloc);

    // generate handle and keep the created texture in  the local map
    TextureHandle handle{ static_cast<uint32_t>(_textures.size()) + 1 };
    _textures.try_emplace(handle,
                          image,
                          imageAlloc,
                          width,
                          height,
                          mipLevels,
                          VK_FORMAT_R8G8B8A8_SRGB,
                          _device,
                          _vmaAllocator);

    return handle;
}

void Renderer::destroyTexture(TextureHandle handle)
{
    _textures.erase(handle);
}

void Renderer::setBlockAtlas(TextureHandle handle)
{
    Texture& tex = _textures.at(handle);
    VkDescriptorImageInfo imgInfo = tex.descriptorInfo();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = _textureDescSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imgInfo;

    vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);
}

void Renderer::initVulkan()
{
    createVulkanInstance();

    _surface = _window.getVulkanSurface(_instance);

    getPhysicalDevice();
    getGraphicsQueue();
    createDevice();
    initVMA();

    // create the upload command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = _gfxQueueFamIdx;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(_device, &poolInfo, nullptr, &_uploadCommandPool) != VK_SUCCESS)
        VoxisLog::critical("Failed to create the upload command pool");

    createSwapchain(_window.width(), _window.height());
    createGrpahicsPipeline();
    createSyncResources();
    createFrameResources();
}

void Renderer::shutdownVulkan()
{
    vkDeviceWaitIdle(_device);

    for (auto& frame : _frameResources)
    {
        vkDestroySemaphore(_device, frame.imageAcquiredSemaphore, nullptr);
        vkDestroyCommandPool(_device, frame.commandPool, nullptr);
        vmaDestroyBuffer(_vmaAllocator, frame.uniformBO, frame.uniformAllocation);
    }

    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(_device, _cameraSetLayout, nullptr);

    vkDestroyDescriptorSetLayout(_device, _textureSetLayout, nullptr);
    vkDestroyCommandPool(_device, _uploadCommandPool, nullptr);

    vkDestroySemaphore(_device, _timelineSemaphore, nullptr);

    vkDestroyPipeline(_device, _pipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipelineLayout, nullptr);

    destroySwapchain(true);

    vmaDestroyAllocator(_vmaAllocator);

    vkDestroyDevice(_device, nullptr);
    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyInstance(_instance, nullptr);
}

void Renderer::createVulkanInstance()
{
    if (volkInitialize() != VK_SUCCESS)
        VoxisLog::critical("Failed to initialize volk");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Voxis";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Voxis";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VULKAN_VERSION;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = _window.getVulkanInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

#ifdef __APPLE__
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

#ifndef NDEBUG
    const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = &validationLayer;
#endif

    if (vkCreateInstance(&createInfo, nullptr, &_instance) != VK_SUCCESS)
        VoxisLog::critical("Failed to create Vulkan instance");

    volkLoadInstance(_instance);

    VoxisLog::info("Vulkan instance created");
}

void Renderer::getPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
    if (deviceCount == 0)
        VoxisLog::critical("No Vulkan capable GPU found");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

    _physicalDevice = devices[0];
    for (auto device : devices)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            _physicalDevice = device;
            break;
        }
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(_physicalDevice, &properties);
    VoxisLog::info("Physical device selected: {}", properties.deviceName);
}

void Renderer::getGraphicsQueue()
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++)
    {
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(_physicalDevice, i, _surface, &presentSupport);

        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
        {
            _gfxQueueFamIdx = i;
            VoxisLog::info("Graphics/present queue family found: {}", _gfxQueueFamIdx);
            return;
        }
    }

    VoxisLog::critical("No suitable graphics/present queue family found");
}

void Renderer::createDevice()
{
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = _gfxQueueFamIdx;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &priority;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.timelineSemaphore = VK_TRUE;
    features12.pNext = &features13;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features12;

    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

#ifdef __APPLE__
    deviceExtensions.push_back("VK_KHR_portability_subset");
#endif

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &features2;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(_physicalDevice, &createInfo, nullptr, &_device) != VK_SUCCESS)
        VoxisLog::critical("Failed to create logical device");

    volkLoadDevice(_device);

    vkGetDeviceQueue(_device, _gfxQueueFamIdx, 0, &_gfxQueue);

    VoxisLog::info("Logical device created");
}

void Renderer::initVMA()
{
    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = _physicalDevice;
    allocatorInfo.device = _device;
    allocatorInfo.instance = _instance;
    allocatorInfo.vulkanApiVersion = VULKAN_VERSION;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    if (vmaCreateAllocator(&allocatorInfo, &_vmaAllocator) != VK_SUCCESS)
        VoxisLog::critical("Failed to create VMA allocator");

    VoxisLog::info("VMA allocator created");
}

void Renderer::createSwapchain(uint32_t width, uint32_t height)
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &capabilities);

    VkExtent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        extent = capabilities.currentExtent;
    }
    else
    {
        extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(_physicalDevice, _surface, &presentModeCount, presentModes.data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto mode : presentModes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            presentMode = mode;
            break;
        }
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainKHR oldSwapchain = _swapchain;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = _surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = SWAPCHAIN_FORMAT;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    if (vkCreateSwapchainKHR(_device, &createInfo, nullptr, &_swapchain) != VK_SUCCESS)
        VoxisLog::critical("Failed to create swapchain");

    if (oldSwapchain)
        vkDestroySwapchainKHR(_device, oldSwapchain, nullptr);

    _scWidth = extent.width;
    _scHeight = extent.height;

    uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(_device, _swapchain, &actualImageCount, nullptr);
    _swapchainImages.resize(actualImageCount);
    vkGetSwapchainImagesKHR(_device, _swapchain, &actualImageCount, _swapchainImages.data());

    _swapchainImageViews.resize(actualImageCount);
    for (uint32_t i = 0; i < actualImageCount; i++)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = _swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = SWAPCHAIN_FORMAT;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        if (vkCreateImageView(_device, &viewInfo, nullptr, &_swapchainImageViews[i]) != VK_SUCCESS)
            VoxisLog::critical("Failed to create swapchain image view");
    }

    VkImageCreateInfo depthImageInfo{};
    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.format = DEPTH_FORMAT;
    depthImageInfo.extent = { extent.width, extent.height, 1 };
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = 1;
    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo depthAllocInfo{};
    depthAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(_vmaAllocator,
                       &depthImageInfo,
                       &depthAllocInfo,
                       &_depthImage,
                       &_depthImageAllocation,
                       nullptr) != VK_SUCCESS)
        VoxisLog::critical("Failed to create depth image");

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = _depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = DEPTH_FORMAT;
    depthViewInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

    if (vkCreateImageView(_device, &depthViewInfo, nullptr, &_depthImageView) != VK_SUCCESS)
        VoxisLog::critical("Failed to create depth image view");

    _renderCompleteSemaphores.resize(actualImageCount);
    for (uint32_t i = 0; i < actualImageCount; i++)
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_renderCompleteSemaphores[i]) != VK_SUCCESS)
            VoxisLog::critical("Failed to create semaphore");
    }

    VoxisLog::info("Swapchain created ({}x{}, {} images)", _scWidth, _scHeight, actualImageCount);
}

void Renderer::destroySwapchain(bool destroySwapchainHandle)
{
    if (_depthImageView)
    {
        vkDestroyImageView(_device, _depthImageView, nullptr);
        _depthImageView = nullptr;
    }
    if (_depthImage)
    {
        vmaDestroyImage(_vmaAllocator, _depthImage, _depthImageAllocation);
        _depthImage = nullptr;
        _depthImageAllocation = nullptr;
    }

    for (auto semaphore : _renderCompleteSemaphores)
        vkDestroySemaphore(_device, semaphore, nullptr);
    _renderCompleteSemaphores.clear();

    for (auto view : _swapchainImageViews)
        vkDestroyImageView(_device, view, nullptr);
    _swapchainImageViews.clear();
    _swapchainImages.clear();

    if (destroySwapchainHandle && _swapchain)
    {
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
        _swapchain = nullptr;
    }
}

void Renderer::recreateSwapchain()
{
    vkDeviceWaitIdle(_device);

    destroySwapchain(false);
    createSwapchain(_window.width(), _window.height());

    _requireSwapchainRecreate = false;
}

VkShaderModule Renderer::createShaderModule(const std::string& filepath, shaderc_shader_kind kind)
{
    std::string source = Voxis::readFile(filepath);
    if (source.empty())
        VoxisLog::critical("Failed to read shader file: {}", filepath);

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(source, kind, filepath.c_str(), options);
    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
        VoxisLog::critical("Shader compilation failed ({}): {}", filepath, result.GetErrorMessage());

    std::vector<uint32_t> spirv(result.cbegin(), result.cend());

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    VkShaderModule shaderModule = nullptr;
    if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        VoxisLog::critical("Failed to create shader module: {}", filepath);

    return shaderModule;
}

void Renderer::createGrpahicsPipeline()
{
    _vertShader = createShaderModule(SHADER_DIR_PATH + "shader.vert", shaderc_glsl_vertex_shader);
    _fragShader = createShaderModule(SHADER_DIR_PATH + "shader.frag", shaderc_glsl_fragment_shader);

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = _vertShader;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = _fragShader;
    fragStageInfo.pName = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> stages{ vertStageInfo, fragStageInfo };

    // Vertex specification
    VkVertexInputBindingDescription vertBindingDesc{};
    vertBindingDesc.stride = sizeof(Vertex);
    vertBindingDesc.binding = 0;
    vertBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> vertAttribDesc{};
    vertAttribDesc[0].location = 0;
    vertAttribDesc[0].binding = 0;
    vertAttribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertAttribDesc[0].offset = offsetof(Vertex, pos);
    vertAttribDesc[1].location = 1;
    vertAttribDesc[1].binding = 0;
    vertAttribDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertAttribDesc[1].offset = offsetof(Vertex, normal);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertBindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertAttribDesc.size());
    vertexInputInfo.pVertexAttributeDescriptions = vertAttribDesc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportStateInfo{};
    viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateInfo.viewportCount = 1;
    viewportStateInfo.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
    rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationInfo.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampleInfo{};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilInfo.depthTestEnable = VK_TRUE;
    depthStencilInfo.depthWriteEnable = VK_TRUE;
    depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
    colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
    dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    std::vector<VkDescriptorPoolSize> poolSizes{ { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT },
                                                 { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 } };
    VkDescriptorPoolCreateInfo descriptorInfo{};
    descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorInfo.maxSets = MAX_FRAMES_IN_FLIGHT + 1;
    descriptorInfo.poolSizeCount = 2;
    descriptorInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(_device, &descriptorInfo, nullptr, &_descriptorPool) != VK_SUCCESS)
        VoxisLog::critical("Failed to create the descriptor pool");

    //===== PIPELINE LAYOUT =====
    // camera uniforms
    VkDescriptorSetLayoutBinding camLayoutBinding{};
    camLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    camLayoutBinding.binding = 0;
    camLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    camLayoutBinding.descriptorCount = 1;

    VkDescriptorSetLayoutCreateInfo camSetlayoutInfo{};
    camSetlayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    camSetlayoutInfo.bindingCount = 1;
    camSetlayoutInfo.pBindings = &camLayoutBinding;

    if (vkCreateDescriptorSetLayout(_device, &camSetlayoutInfo, nullptr, &_cameraSetLayout) != VK_SUCCESS)
        VoxisLog::critical("Failed to create the camera descriptor set layout");

    // texture
    VkDescriptorSetLayoutBinding texBinding{};
    texBinding.binding = 0;
    texBinding.descriptorCount = 1;
    texBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo texSetLayoutInfo{};
    texSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    texSetLayoutInfo.bindingCount = 1;
    texSetLayoutInfo.pBindings = &texBinding;

    if (vkCreateDescriptorSetLayout(_device, &texSetLayoutInfo, nullptr, &_textureSetLayout) != VK_SUCCESS)
        VoxisLog::critical("Failed to create the texture descriptor set layout");

    VkDescriptorSetAllocateInfo descSetAllocInfo{};
    descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAllocInfo.descriptorPool = _descriptorPool;
    descSetAllocInfo.descriptorSetCount = 1;
    descSetAllocInfo.pSetLayouts = &_textureSetLayout;
    if (vkAllocateDescriptorSets(_device, &descSetAllocInfo, &_textureDescSet) != VK_SUCCESS)
        VoxisLog::critical("Failed to allocate the texture descriptor set");

    // push constant range, only model matriux for now
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::mat4);

    std::vector<VkDescriptorSetLayout> setLayouts{ _cameraSetLayout, _textureSetLayout };
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    layoutInfo.setLayoutCount = 2;
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if (vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &_pipelineLayout) != VK_SUCCESS)
        VoxisLog::critical("Failed to create pipeline layout");

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &SWAPCHAIN_FORMAT;
    renderingInfo.depthAttachmentFormat = DEPTH_FORMAT;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportStateInfo;
    pipelineInfo.pRasterizationState = &rasterizationInfo;
    pipelineInfo.pMultisampleState = &multisampleInfo;
    pipelineInfo.pDepthStencilState = &depthStencilInfo;
    pipelineInfo.pColorBlendState = &colorBlendInfo;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    pipelineInfo.layout = _pipelineLayout;

    if (vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_pipeline) != VK_SUCCESS)
        VoxisLog::critical("Failed to create graphics pipeline");

    vkDestroyShaderModule(_device, _vertShader, nullptr);
    vkDestroyShaderModule(_device, _fragShader, nullptr);
    _vertShader = nullptr;
    _fragShader = nullptr;

    VoxisLog::info("Graphics pipeline created");
}

void Renderer::createSyncResources()
{
    VkSemaphoreTypeCreateInfo timelineInfo{};
    timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineInfo.initialValue = MAX_FRAMES_IN_FLIGHT;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = &timelineInfo;

    if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_timelineSemaphore) != VK_SUCCESS)
        VoxisLog::critical("Failed to create timeline semaphore");

    VoxisLog::info("Sync resources created");
}

void Renderer::createFrameResources()
{
    for (auto& frame : _frameResources)
    {
        // command pool
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = _gfxQueueFamIdx;

        if (vkCreateCommandPool(_device, &poolInfo, nullptr, &frame.commandPool) != VK_SUCCESS)
            VoxisLog::critical("Failed to create command pool");

        // command buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = frame.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(_device, &allocInfo, &frame.commandBuffer) != VK_SUCCESS)
            VoxisLog::critical("Failed to allocate command buffer");

        // image acquired semaphore
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &frame.imageAcquiredSemaphore) != VK_SUCCESS)
            VoxisLog::critical("Failed to create semaphore");

        // uniforms buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = 2 * sizeof(glm::mat4);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo uniformnsAllocCreateInfo{};
        uniformnsAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
        uniformnsAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                                         VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo uniformAllocInfo{};
        if (vmaCreateBuffer(_vmaAllocator,
                            &bufferInfo,
                            &uniformnsAllocCreateInfo,
                            &frame.uniformBO,
                            &frame.uniformAllocation,
                            &uniformAllocInfo) != VK_SUCCESS)
            VoxisLog::critical("Failed to create uniforms buffer");

        frame.uniformsMapped = uniformAllocInfo.pMappedData;

        // descriptor set
        VkDescriptorSetAllocateInfo descSetAllocInfo{};
        descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descSetAllocInfo.descriptorSetCount = 1;
        descSetAllocInfo.descriptorPool = _descriptorPool;
        descSetAllocInfo.pSetLayouts = &_cameraSetLayout;

        if (vkAllocateDescriptorSets(_device, &descSetAllocInfo, &frame.uniformDescriptorSet) != VK_SUCCESS)
            VoxisLog::critical("Failed to allocate the frame descriptor set");

        // link to the buffer
        VkDescriptorBufferInfo descBufferInfo{};
        descBufferInfo.buffer = frame.uniformBO;
        descBufferInfo.offset = 0;
        descBufferInfo.range = 2 * sizeof(glm::mat4);

        VkWriteDescriptorSet writeDescSet{};
        writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescSet.dstSet = frame.uniformDescriptorSet;
        writeDescSet.dstBinding = 0;
        writeDescSet.descriptorCount = 1;
        writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescSet.dstArrayElement = 0;
        writeDescSet.pBufferInfo = &descBufferInfo;

        vkUpdateDescriptorSets(_device, 1, &writeDescSet, 0, nullptr);
    }

    VoxisLog::info("Frame resources created");
}

void Renderer::getNextImageIndex(FrameResources& frame, uint32_t* imageIndex)
{
    if (_window.width() == 0 || _window.height() == 0)
        return;

    uint64_t waitValue = _nextSignalValue - MAX_FRAMES_IN_FLIGHT;
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &_timelineSemaphore;
    waitInfo.pValues = &waitValue;
    vkWaitSemaphores(_device, &waitInfo, UINT64_MAX);

    if (_requireSwapchainRecreate)
        recreateSwapchain();

    VkResult acquireResult = vkAcquireNextImageKHR(_device,
                                                   _swapchain,
                                                   UINT64_MAX,
                                                   frame.imageAcquiredSemaphore,
                                                   VK_NULL_HANDLE,
                                                   imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        _requireSwapchainRecreate = true;
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        VoxisLog::critical("Failed to acquire swapchain image");
}

void Renderer::recordFrame(FrameResources& frame,
                           uint32_t imageIndex,
                           std::span<std::pair<MeshHandle, glm::mat4>> meshes)
{
    vkResetCommandPool(_device, frame.commandPool, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(frame.commandBuffer, &beginInfo);

    std::array<VkImageMemoryBarrier2, 2> toAttachmentBarriers{};
    toAttachmentBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toAttachmentBarriers[0].srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    toAttachmentBarriers[0].srcAccessMask = 0;
    toAttachmentBarriers[0].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toAttachmentBarriers[0].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toAttachmentBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toAttachmentBarriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toAttachmentBarriers[0].image = _swapchainImages[imageIndex];
    toAttachmentBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    toAttachmentBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toAttachmentBarriers[1].srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    toAttachmentBarriers[1].srcAccessMask = 0;
    toAttachmentBarriers[1].dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    toAttachmentBarriers[1].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    toAttachmentBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toAttachmentBarriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    toAttachmentBarriers[1].image = _depthImage;
    toAttachmentBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

    VkDependencyInfo toAttachmentDependency{};
    toAttachmentDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    toAttachmentDependency.imageMemoryBarrierCount = static_cast<uint32_t>(toAttachmentBarriers.size());
    toAttachmentDependency.pImageMemoryBarriers = toAttachmentBarriers.data();
    vkCmdPipelineBarrier2(frame.commandBuffer, &toAttachmentDependency);

    VkRenderingAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachmentInfo.imageView = _swapchainImageViews[imageIndex];
    colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentInfo.clearValue.color = { { 0.01f, 0.01f, 0.02f, 1.0f } };

    VkRenderingAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachmentInfo.imageView = _depthImageView;
    depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = { 0, 0 };
    renderingInfo.renderArea.extent = { _scWidth, _scHeight };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachmentInfo;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    vkCmdBeginRendering(frame.commandBuffer, &renderingInfo);

    vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);

    VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(_scWidth), static_cast<float>(_scHeight), 0.0f, 1.0f };
    vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{ { 0, 0 }, { _scWidth, _scHeight } };
    vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);

    std::array<VkDescriptorSet, 2> sets{ frame.uniformDescriptorSet, _textureDescSet };
    vkCmdBindDescriptorSets(frame.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            _pipelineLayout,
                            0,
                            static_cast<uint32_t>(sets.size()),
                            sets.data(),
                            0,
                            nullptr);

    for (auto mesh : meshes)
    {
        Mesh& m = _meshes.at(mesh.first);
        glm::mat4 modelMat = mesh.second;

        VkBuffer vertBuffer = m.vertBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &vertBuffer, &offset);

        vkCmdBindIndexBuffer(frame.commandBuffer, m.idxBuffer(), 0, VK_INDEX_TYPE_UINT32);

        vkCmdPushConstants(frame.commandBuffer,
                           _pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(modelMat),
                           &modelMat);

        vkCmdDrawIndexed(frame.commandBuffer, m.idxCount(), 1, 0, 0, 0);
    }

    vkCmdEndRendering(frame.commandBuffer);

    VkImageMemoryBarrier2 toPresentBarrier{};
    toPresentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    toPresentBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    toPresentBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    toPresentBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    toPresentBarrier.dstAccessMask = 0;
    toPresentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toPresentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresentBarrier.image = _swapchainImages[imageIndex];
    toPresentBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkDependencyInfo toPresentDependency{};
    toPresentDependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    toPresentDependency.imageMemoryBarrierCount = 1;
    toPresentDependency.pImageMemoryBarriers = &toPresentBarrier;
    vkCmdPipelineBarrier2(frame.commandBuffer, &toPresentDependency);

    vkEndCommandBuffer(frame.commandBuffer);
}

void Renderer::submitFrame(FrameResources& frame, uint32_t imageIndex)
{
    VkSemaphoreSubmitInfo waitSemaphoreInfo{};
    waitSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreInfo.semaphore = frame.imageAcquiredSemaphore;
    waitSemaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkCommandBufferSubmitInfo cmdSubmitInfo{};
    cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    cmdSubmitInfo.commandBuffer = frame.commandBuffer;

    std::array<VkSemaphoreSubmitInfo, 2> signalSemaphoreInfos{};
    signalSemaphoreInfos[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfos[0].semaphore = _renderCompleteSemaphores[imageIndex];
    signalSemaphoreInfos[0].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    signalSemaphoreInfos[1].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreInfos[1].semaphore = _timelineSemaphore;
    signalSemaphoreInfos[1].value = _nextSignalValue;
    signalSemaphoreInfos[1].stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = 1;
    submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfo;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdSubmitInfo;
    submitInfo.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfos.size());
    submitInfo.pSignalSemaphoreInfos = signalSemaphoreInfos.data();

    if (vkQueueSubmit2(_gfxQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        VoxisLog::critical("Failed to submit draw command buffer");

    _nextSignalValue++;
}

void Renderer::presentFrame(FrameResources& frame, uint32_t imageIndex)
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &_renderCompleteSemaphores[imageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(_gfxQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
        _requireSwapchainRecreate = true;
    else if (presentResult != VK_SUCCESS)
        VoxisLog::critical("Failed to present swapchain image");
}

VkCommandBuffer Renderer::beginOneShotCommands()
{
    VkCommandBufferAllocateInfo bufferAllocInfo{};
    bufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferAllocInfo.commandPool = _uploadCommandPool;
    bufferAllocInfo.commandBufferCount = 1;
    bufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer cmdBuffer = nullptr;
    if (vkAllocateCommandBuffers(_device, &bufferAllocInfo, &cmdBuffer) != VK_SUCCESS)
        VoxisLog::critical("Failed to allocate a one shot comand buffer");

    VkCommandBufferBeginInfo buffBeginInfo{};
    buffBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    buffBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdBuffer, &buffBeginInfo);

    return cmdBuffer;
}

void Renderer::endOneShotCommands(VkCommandBuffer cmdBuffer)
{
    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(_gfxQueue, 1, &submitInfo, nullptr);

    vkQueueWaitIdle(_gfxQueue);

    vkFreeCommandBuffers(_device, _uploadCommandPool, 1, &cmdBuffer);
}
