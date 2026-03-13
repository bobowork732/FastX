# FastX

FastX is a **custom C++ real-time software renderer** (no OpenGL) that draws fully on the CPU.

## Features implemented

- CPU rasterizer with depth buffer (faces behind faces are invisible).
- Back-face and behind-camera culling (object/face behind camera is invisible).
- Smooth lighting shader (Lambert + specular).
- Shadow darkening on ground contact.
- Screen-space ray-trace inspired reflection pass (SSR-style post effect).
- OBJ import (loaded first from a simple console file explorer list).
- Procedural primitives automatically added:
  - Cube
  - Sphere
- Real-time camera movement:
  - `W/A/S/D`: move
  - `Q/E`: move down/up
  - Hold right mouse: look around smoothly
  - `Esc`: quit

## Build

Requirements:
- CMake 3.16+
- C++20 compiler
- SDL2 development package

```bash
cmake -S . -B build
cmake --build build -j
```

## Run

From repo root:

```bash
./build/FastX
```

At launch, FastX scans the current folder for `.obj` files and lets you select one to import first.
If no OBJ is selected, it still starts with a cube and sphere scene.

## Rendering pipeline (fast path)

1. Transform vertices (model → view → projection).
2. Cull geometry behind camera.
3. Back-face cull triangles.
4. Rasterize only triangle screen-space bounding boxes.
5. Z-test with depth buffer.
6. Shade per-pixel with smooth normals.
7. Apply screen-space reflection pass.

This keeps rendering significantly faster than naive full-screen per-triangle approaches.
