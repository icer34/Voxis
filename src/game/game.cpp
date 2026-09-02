#include "game.h"

#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

Game::Game() {}

void Game::run()
{
    init();

    while (!_window->shouldClose())
    {
        update();

        render();
    }

    shutdown();
}

void Game::init()
{
    _window = std::make_unique<Window>(1600, 900, "Voxis");
    _camera = std::make_unique<Camera>(glm::vec3(0.0f, 20.0f, 20.0f));
    _renderer = std::make_unique<Renderer>(*_window);
    _blockAtlas = std::make_unique<BlockAtlas>(*_renderer);
    _blockRegistry = std::make_unique<BlockRegistry>();
    _world = std::make_unique<World>(67u, *_blockRegistry, *_blockAtlas, *_renderer);

    _lastFrameTime = _window->getTime();
}

void Game::update()
{
    if (_window->consumeKeyPress(Key::Esc))
    {
        _window->setCursorEnabled(!_window->isCursorEnabled());
    }

    double now = _window->getTime();
    _dt = now - _lastFrameTime;
    _lastFrameTime = now;

    float dx = static_cast<float>(_window->consumeDx());
    float dy = static_cast<float>(_window->consumeDy());

    if (!_window->isCursorEnabled())
    {
        constexpr float MOVE_SPEED = 20.0f;

        glm::vec3 moveDir(0.0f);
        if (_window->isKeyPressed(Key::W))
            moveDir += _camera->getFront();
        if (_window->isKeyPressed(Key::S))
            moveDir -= _camera->getFront();
        if (_window->isKeyPressed(Key::D))
            moveDir += _camera->getRight();
        if (_window->isKeyPressed(Key::A))
            moveDir -= _camera->getRight();
        if (_window->isKeyPressed(Key::Space))
            moveDir += glm::vec3(0.0f, 1.0f, 0.0f);
        if (_window->isKeyPressed(Key::LShift))
            moveDir -= glm::vec3(0.0f, 1.0f, 0.0f);

        if (glm::length(moveDir) > 0.0f)
            _camera->move(glm::normalize(moveDir) * MOVE_SPEED * static_cast<float>(_dt));

        _camera->rotate(dx, dy);
    }

    _window->update();

    // fixed-rate logic ticks: accumulate real time elapsed and drain it in TICK_DT increments, so
    // world logic always advances by the same amount per tick regardless of the render framerate
    _tickAccumulator += _dt;
    _tickAccumulator = std::min(_tickAccumulator, MAX_ACCUMULATED_TIME);

    while (_tickAccumulator >= TICK_DT)
    {
        tick();
        _tickAccumulator -= TICK_DT;
    }
}

void Game::tick()
{
    double tickStart = _window->getTime();
    _world->update(TICK_DT, _camera->getPos());
    _hud.recordTick(_window->getTime() - tickStart);
}

void Game::render()
{
    _renderer->beginUIFrame();
    _hud.draw(_dt);

    std::vector<std::pair<const Chunk*, PushConstants>> renderData;
    for (auto chunk : _world->getRenderableChunks())
    {
        PushConstants pc;
        glm::vec3 worldPos = glm::vec3(chunk->coords() * static_cast<int>(Chunk::SIZE));
        pc.modelMat = glm::translate(glm::mat4(1.0f), worldPos);
        pc.blockAtlasTextureIndex = _blockAtlas->handle().value;
        pc.chunkDataTextureIndex = chunk->dataTextureHandle().value;
        pc.atlasTilesPerRow = _blockAtlas->tilesPerRow();

        renderData.push_back({ chunk, pc });
    }

    _renderer->render(*_camera, renderData);
}

void Game::shutdown() {}