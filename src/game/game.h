#pragma once

#include <memory>

#include "platform/window.h"
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
    double _dt = 0.0;

    // fixed-rate world/game logic ticks, independent of (and slower than) the render framerate
    constexpr static double TICK_RATE = 20.0;
    constexpr static double TICK_DT = 1.0 / TICK_RATE;
    constexpr static double MAX_ACCUMULATED_TIME = 0.25; // caps catch-up ticks after a long stall
    double _tickAccumulator = 0.0;

    void init();
    void update();
    void tick();
    void render();
    void shutdown();
};