#include "game.h"

#include "graphics/chunk_mesher.h"
#include "graphics/mesh.h"
#include "chunk.h"

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

    _lastFrameTime = _window->getTime();

    // generate a test chunk
    Chunk c{ { 0, 0 } };
    for (uint32_t x = 0; x < Chunk::SIZE; x++)
    {
        for (uint32_t y = 0; y < Chunk::SIZE; y++)
        {
            for (uint32_t z = 0; z < Chunk::SIZE; z++)
            {
                c.setBlock({ x, y, z }, 1);
            }
        }
    }
    MeshData data = ChunkMesher::getMeshData(c, _blockRegistry, *_blockAtlas);
    _testMeshHandle = _renderer->createMesh(data.vertices, data.indices);
    c.setMeshHandle(_testMeshHandle.value);
}

void Game::update()
{
    if (_window->consumeKeyPress(Key::Esc))
    {
        _window->setCursorEnabled(!_window->isCursorEnabled());
    }

    double now = _window->getTime();
    float dt = static_cast<float>(now - _lastFrameTime);
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
            _camera->move(glm::normalize(moveDir) * MOVE_SPEED * dt);

        _camera->rotate(dx, dy);
    }

    _window->update();
}

void Game::render()
{
    PushConstants pc{ _testModelMat, _blockAtlas->handle().value };
    auto renderData = std::vector{ std::pair{ _testMeshHandle, pc } };
    _renderer->render(*_camera, renderData);
}

void Game::shutdown() {}