cmake_minimum_required(VERSION 3.24)

project(RaylibMultiplayerScaffold
    VERSION 0.1.0
    DESCRIPTION "Server-authoritative Raylib multiplayer scaffold with EnTT ECS"
    LANGUAGES CXX
)

option(SCAFFOLD_BUILD_CLIENT "Build the Raylib client executable" ON)
option(SCAFFOLD_BUILD_SERVER "Build the headless server executable" ON)
option(SCAFFOLD_BUILD_TESTS "Build the scaffold test executable" ON)
option(SCAFFOLD_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(SCAFFOLD_ENABLE_CLANG_TIDY "Run clang-tidy for scaffold targets when available" OFF)
option(SCAFFOLD_ENABLE_ASAN "Enable AddressSanitizer for scaffold targets on supported toolchains" OFF)
option(SCAFFOLD_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer for scaffold targets on supported toolchains" OFF)
option(SCAFFOLD_ENABLE_MSVC_ANALYSIS "Enable /analyze for scaffold targets when using MSVC" OFF)
option(SCAFFOLD_FETCH_DEPS "Fetch missing third-party dependencies with CMake FetchContent" ON)
option(SCAFFOLD_FETCH_RAYLIB "Fetch Raylib when it is not already installed" ON)
option(SCAFFOLD_ENABLE_RTC_TRANSPORT "Enable native libdatachannel/KCP transport integration target" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerOptions)
include(Dependencies)

scaffold_configure_tooling()

add_subdirectory(GameLogic)
add_subdirectory(Networking)

if(SCAFFOLD_BUILD_SERVER)
    add_subdirectory(Server)
endif()

if(SCAFFOLD_BUILD_CLIENT)
    add_subdirectory(Client)
endif()

if(SCAFFOLD_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
