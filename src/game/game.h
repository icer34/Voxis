#pragma once

#include <memory>

#include "plateform/window.h"
#include "graphics/renderer.h"
#include "graphics/camera.h"
#include "graphics/block_atlas.h"
#include "block_registry.h"

/**
 * @brief Owns the window, renderer and camera, and drives the main game loop
 *
 */
class Game
{
public:
    Game();

    void run();

private:
    std::unique_ptr<Window> _window = nullptr;
    std::unique_ptr<Renderer> _renderer = nullptr;
    std::unique_ptr<Camera> _camera = nullptr;
    std::unique_ptr<BlockAtlas> _blockAtlas = nullptr;
    BlockRegistry _blockRegistry;

    MeshHandle _testMeshHandle{ 0 };
    glm::mat4 _testModelMat = glm::mat4(1.0f);

    double _lastFrameTime = 0.0;

    void init();
    void update();
    void render();
    void shutdown();
};