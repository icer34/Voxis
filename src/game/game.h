#pragma once

#include <memory>

#include "plateform/window.h"
#include "graphics/renderer.h"
#include "graphics/camera.h"
#include "graphics/block_atlas.h"
#include "block_registry.h"
#include "chunk.h"
#include "world.h"
#include "hud.h"

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
    std::unique_ptr<BlockRegistry> _blockRegistry = nullptr;
    std::unique_ptr<World> _world = nullptr;
    Hud _hud;

    double _lastFrameTime = 0.0;
    float _dt = 0.0f;

    void init();
    void update();
    void render();
    void shutdown();
};