#pragma once

#pragma GCC system_header
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <shaderc/shaderc.hpp>

#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <span>

#include "mesh.h"
#include "texture.h"

// Handle types definition
template <typename Tag> struct Handle
{
    uint32_t value = 0;
    bool operator==(const Handle&) const = default;
};
namespace std
{
template <typename Tag> struct hash<Handle<Tag>>
{
    size_t operator()(const Handle<Tag>& h) const noexcept { return std::hash<uint32_t>{}(h.value); };
};
} // namespace std
using MeshHandle = Handle<struct MeshTag>;
using TextureHandle = Handle<struct TextureTag>;

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

// forward declarations
struct VmaAllocator_T;
typedef struct VmaAllocator_T* VmaAllocator;
struct VmaAllocation_T;
typedef struct VmaAllocation_T* VmaAllocation;
class Window;
class Camera;

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
     * @brief Create a Texture object, allocates all the gpu resources needed
     * ONLY FORMAT SUPPORTED : R8_G8_B8_A8
     *
     * @param width
     * @param height
     * @param mipLevels
     * @param data array of bytes representing the image for each mipLevel desired
     * @return the TextureHandle
     */
    TextureHandle createTexture(uint32_t width, uint32_t height, uint32_t mipLevels, const unsigned char** data);

    /**
     * @brief Deallocates the GPU resources for the texture
     *
     * @param handle
     */
    void destroyTexture(TextureHandle handle);

    /**
     * @brief Assign a previously created texture to the block atlas
     *
     * @param handle
     */
    void setBlockAtlas(TextureHandle handle);

    /**
     * @brief Renders the desired meshes for the given camera
     *
     * @param cam
     * @param meshes list of <meshHandle, modelMatrix> to be rendered
     */
    void render(Camera& cam, std::span<std::pair<MeshHandle, glm::mat4>> meshes);

private:
    constexpr static uint32_t VULKAN_VERSION{ VK_API_VERSION_1_4 };
    constexpr static uint32_t MAX_FRAMES_IN_FLIGHT{ 2 };
    constexpr static VkFormat SWAPCHAIN_FORMAT{ VK_FORMAT_B8G8R8A8_SRGB };
    constexpr static VkFormat DEPTH_FORMAT{ VK_FORMAT_D32_SFLOAT };
    constexpr static std::string SHADER_DIR_PATH{ "src/shaders/" };

    Window& _window;
    std::unordered_map<MeshHandle, Mesh> _meshes;
    std::unordered_map<TextureHandle, Texture> _textures;

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
    VkDescriptorSetLayout _textureSetLayout = nullptr;
    VkDescriptorSet _textureDescSet = nullptr;

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

    void recreateSwapchain();

    // one shot command buffers methods
    VkCommandBuffer beginOneShotCommands();
    void endOneShotCommands(VkCommandBuffer cmdBuffer);

    // cleanup
    void destroySwapchain(bool destroySwapchainHandle = true);
    void shutdownVulkan();

    // render helpers
    void getNextImageIndex(FrameResources& frame, uint32_t* imageIndex);
    void recordFrame(FrameResources& frame, uint32_t imageIndex, std::span<std::pair<MeshHandle, glm::mat4>> meshes);
    void submitFrame(FrameResources& frame, uint32_t imageIndex);
    void presentFrame(FrameResources& frame, uint32_t imageIndex);
};