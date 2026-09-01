#include "game.h"

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
    _dt = static_cast<float>(now - _lastFrameTime);
    _lastFrameTime = now;

    float dx = static_cast<float>(_window->consumeDx());
    float dy = static_cast<float>(_window->consumeDy());

    if (!_window->isCursorEnabled())
    {
        constexpr float MOVE_SPEED = 5.0f;

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
            moveDir += _camera->getUp();
        if (_window->isKeyPressed(Key::LShift))
            moveDir -= _camera->getUp();

        if (glm::length(moveDir) > 0.0f)
            _camera->move(glm::normalize(moveDir) * MOVE_SPEED * _dt);

        _camera->rotate(dx, dy);
    }

    _window->update();
}

void Game::render()
{
    _renderer->beginUIFrame();
    _hud.draw(_dt);

    std::vector<std::pair<const Chunk*, PushConstants>> renderData;
    for (auto chunk : _world->getChunks())
    {
        PushConstants pc;
        glm::vec3 worldPos = glm::vec3(chunk->coords() * static_cast<int>(Chunk::SIZE));
        pc.modelMat = glm::translate(glm::mat4(1.0f), worldPos);
        pc.blockAtlasTextureIndex = _blockAtlas->handle().value;
        pc.chunkDataTextureIndex = chunk->dataTextureHandle().value;

        renderData.push_back({ chunk, pc });
    }

    _renderer->render(*_camera, renderData);
}

void Game::shutdown() {}