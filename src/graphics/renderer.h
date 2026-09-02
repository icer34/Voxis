#pragma once

#include "vk_common.h"
#include <shaderc/shaderc.hpp>

#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <span>

#include "mesh.h"
#include "texture.h"
#include "deletion_queue.h"
#include "handle.h"

/**
 * @brief gathers all the resources that need to be updated every frame
 *
 */
struct FrameResources
{
    VkCommandPool commandPool = nullptr;
    VkCommandBuffer commandBuffer = nullptr;
    VkSemaphore imageAcquiredSemaphore = nullptr;

    VkBuffer uniformBO = nullptr;
    VmaAllocation uniformAllocation = nullptr;
    VkDescriptorSet uniformDescriptorSet = nullptr;
    void* uniformsMapped = nullptr;
};

/**
 * @brief Groups all the constants that are sent to the shaders that may vary for each draw call (ie. not only once per
 * frame)
 *
 */
struct PushConstants
{
    glm::mat4 modelMat;
    uint32_t blockAtlasTextureIndex;
    uint32_t chunkDataTextureIndex;
    // BlockAtlas discovers this at runtime (scans the texture folder), after shaders are already
    // compiled, so it can't be a shader compile-time constant like TEXTURE_SIZE/PADDING/CELL_STRIDE
    uint32_t atlasTilesPerRow;
};

// forward declarations
class Window;
class Camera;
class Chunk;

/**
 * @brief Vulkan renderer: owns the swapchain and pipeline, and manages meshes, textures and frame rendering
 *
 */
class Renderer
{
public:
    Renderer(Window& window);
    ~Renderer();

    /**
     * @brief Create a Mesh object, allocates the GPU resources needed
     *
     * @param vertices
     * @param indices
     * @return the mesh handle to specify when calling render
     */

    MeshHandle createMesh(std::span<const Vertex> vertices, std::span<const uint32_t> indices);

    /**
     * @brief Deallocates the resources of the given mesh
     *
     * @param handle
     */
    void destroyMesh(MeshHandle handle);

    /**
     * @brief Create a Texture object, allocates all the GPU resources needed
     *
     * @param width
     * @param height
     * @param mipLevels
     * @param format the format of the pixel data (and of the created image)
     * @param bytesPerTexel must match format (ex: 4 for R8G8B8A8, 2 for R16_UINT)
     * @param data array of bytes representing the image for each mipLevel desired
     * @return the Texture2DHandle
     */
    Texture2DHandle createTexture2D(uint32_t width,
                                    uint32_t height,
                                    uint32_t mipLevels,
                                    VkFormat format,
                                    uint32_t bytesPerTexel,
                                    const unsigned char** data);

    /**
     * @brief Create a texture object, allocates all the GPU resources needed
     *
     * @param size
     * @param mipLevels
     * @param format the format of the pixel data
     * @param bytesPerTexel must match format (ex: 4 for R8G8B8A8, 2 for R16_UINT)
     * @param data array of bytes representing the image for each mipLevel required
     * @return the Texture3DHandle
     */
    Texture3DHandle createTexture3D(glm::ivec3 size,
                                    uint32_t mipLevels,
                                    VkFormat format,
                                    uint32_t bytesPerTexel,
                                    const unsigned char** data);

    /**
     * @brief Deallocates the GPU resources for a 2D texture
     *
     * @param handle
     */
    void destroyTexture2D(Texture2DHandle handle);

    /**
     * @brief Deallocates the GPU resources for a 3D texture
     *
     * @param handle
     */
    void destroyTexture3D(Texture3DHandle handle);

    /**
     * @brief Renders the desired chunks for the given camera
     *
     * @param cam
     * @param chunks list of <chunk, pushConstants> to be rendered - the mesh and data texture to use
     * are pulled from each chunk directly (blockAtlasTextureIndex/modelMat must still be set by the
     * caller on the given PushConstants, chunkDataTextureIndex is overwritten from the chunk)
     */
    void render(Camera& cam, std::span<std::pair<const Chunk*, PushConstants>> chunks);

    /**
     * @brief Starts a new ImGui frame - call once per frame, before building any ImGui widgets
     * (e.g. via Hud::draw), and before render()
     */
    void beginUIFrame();

private:
    constexpr static uint32_t VULKAN_VERSION = VK_API_VERSION_1_4;
    constexpr static uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    constexpr static VkFormat SWAPCHAIN_FORMAT = VK_FORMAT_B8G8R8A8_SRGB;
    constexpr static VkFormat DEPTH_FORMAT = VK_FORMAT_D32_SFLOAT;
    constexpr static std::string SHADER_DIR_PATH = "src/shaders/";
    constexpr static uint32_t MAX_BINDLESS_TEXTURES = 4096;

    Window& _window;
    std::unordered_map<MeshHandle, Mesh> _meshes;
    uint32_t _nextMeshHandleValue = 1;
    DeletionQueue _deletionQueue;
    std::unordered_map<Texture2DHandle, Texture> _textures2D;
    std::unordered_map<Texture3DHandle, Texture> _textures3D;
    std::vector<uint32_t> _free2DTextureSlots;
    std::vector<uint32_t> _free3DTextureSlots;

    // Vulkan core
    VkInstance _instance = nullptr;
    VkPhysicalDevice _physicalDevice = nullptr;
    VkDevice _device = nullptr;
    VkSurfaceKHR _surface = nullptr;
    VmaAllocator _vmaAllocator = nullptr;

    VkCommandPool _uploadCommandPool = nullptr;

    // Vulkan queue
    uint32_t _gfxQueueFamIdx = 0;
    VkQueue _gfxQueue = nullptr;

    // Vulkan swapchain
    VkSwapchainKHR _swapchain = nullptr;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    std::vector<VkSemaphore> _renderCompleteSemaphores;
    bool _requireSwapchainRecreate = false;
    uint32_t _scWidth = 0;
    uint32_t _scHeight = 0;

    VkImage _depthImage = nullptr;
    VkImageView _depthImageView = nullptr;
    VmaAllocation _depthImageAllocation = nullptr;

    // Vulkan pipeline
    VkPipelineLayout _pipelineLayout = nullptr;
    VkPipeline _pipeline = nullptr;
    VkDescriptorPool _descriptorPool = nullptr;
    VkDescriptorSetLayout _cameraSetLayout = nullptr;

    // texture related
    VkDescriptorSetLayout _bindless2DSetLayout = nullptr;
    VkDescriptorSet _bindless2DSet = nullptr;

    VkDescriptorSetLayout _bindless3DSetLayout = nullptr;
    VkDescriptorSet _bindless3DSet = nullptr;

    // Shader resources
    VkShaderModule _vertShader = nullptr;
    VkShaderModule _fragShader = nullptr;

    // frame and sync resources
    VkSemaphore _timelineSemaphore = nullptr;
    std::array<FrameResources, MAX_FRAMES_IN_FLIGHT> _frameResources;
    uint64_t _frameIndex = 0;
    uint64_t _nextSignalValue = MAX_FRAMES_IN_FLIGHT + 1;

    // initialization
    void initVulkan();
    void createVulkanInstance();
    void getPhysicalDevice();
    void getGraphicsQueue();
    void createDevice();
    void createSwapchain(uint32_t width, uint32_t height);
    VkShaderModule createShaderModule(const std::string& filepath, shaderc_shader_kind kind);
    void createGrpahicsPipeline();
    void createSyncResources();
    void createFrameResources();
    void initVMA();
    void initImGui();

    void recreateSwapchain();

    // one shot command buffers methods
    VkCommandBuffer beginOneShotCommands();
    void endOneShotCommands(VkCommandBuffer cmdBuffer);

    // cleanup
    void destroySwapchain(bool destroySwapchainHandle = true);
    void shutdownVulkan();

    // render helpers
    void getNextImageIndex(FrameResources& frame, uint32_t* imageIndex);
    void
    recordFrame(FrameResources& frame, uint32_t imageIndex, std::span<std::pair<const Chunk*, PushConstants>> chunks);
    void submitFrame(FrameResources& frame, uint32_t imageIndex);
    void presentFrame(FrameResources& frame, uint32_t imageIndex);
};