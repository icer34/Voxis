# %Voxis {#mainpage}

<p align="center">
  <img src="logo.png" alt="Voxis logo" width="400"/>
</p>

%Voxis is a small Minecraft-like voxel RPG game, written in C++20.

## Architecture

The project is organized into three main parts, under `src/`:

- **game/** — game logic: `Game`, `World`, `Chunk`
- **graphics/** — Vulkan rendering: `Renderer`, `Camera`, `Mesh`, `Texture`, `ChunkMesher`
- **plateform/** — window and input (everything plateform specific): `Window`

## Tech stack

- **Vulkan** (via [volk](https://github.com/zeux/volk)) for graphics rendering
- **SDL2** for windowing and input
- **glm** for math (vectors, matrices)
- **EnTT** for the ECS
- **VulkanMemoryAllocator (VMA)** for GPU memory management
- **shaderc** for shader compilation
- **spdlog** for logging

## Building

The project uses CMake (>= 3.24) and CMake Presets.

```sh
cmake --preset <preset>
cmake --build --preset <preset>
```

Dependencies are fetched automatically via `cmake/Dependencies.cmake`.

## Documentation

This documentation is generated with Doxygen from the comments in the source code (`src/`).
