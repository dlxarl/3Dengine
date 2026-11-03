cmake_minimum_required(VERSION 3.20)
project(3D_Engine VERSION 1.0.0 LANGUAGES CXX C)

if (APPLE)
    enable_language(OBJC OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(ENGINE_BUILD_DEMO "Build the 3D_Engine demo executable" ON)
option(ENGINE_BUILD_EDITOR "Build the Editor application" ON)
option(ENGINE_BUILD_TESTS "Build the test executables" ON)

include(cmake/Dependencies.cmake)
include(cmake/EngineResources.cmake)

add_subdirectory(engine)

if (ENGINE_BUILD_DEMO)
    add_subdirectory(demo)
endif()

if (ENGINE_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if (ENGINE_BUILD_TESTS AND ENGINE_BUILD_EDITOR)
    enable_testing()
    add_subdirectory(tests)
endif()
