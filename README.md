# 3D Engine

## About
`3D Engine` — це навчальний 3D-рушій на C++ з рендерингом через OpenGL.  
Проєкт містить базові 3D-примітиви, керування камерою/гравцем, skybox, постобробку та демо-сцени.

## Features
- Рендеринг 3D-об’єктів (Cube, Plane, Sphere, Cylinder)
- Підтримка сцен (`DemoScene`, `DemoPhysics`)
- Шейдери освітлення та тіней
- Skybox і текстури
- Постобробка (motion blur)

## Requirements
- CMake 3.20+
- C++17 компілятор
- OpenGL-сумісне середовище

## Build
```bash
cmake -S . -B build
cmake --build build
```

## Run
```bash
./build/3D_Engine
```

## Project structure
- `src/` — вихідний код рушія та сцен
- `include/` — заголовочні файли
- `assets/` — текстури, skybox, HUD

