include(FetchContent)

find_package(EnTT QUIET)
if(NOT EnTT_FOUND AND SCAFFOLD_FETCH_DEPS)
    FetchContent_Declare(
        entt
        GIT_REPOSITORY https://github.com/skypjack/entt.git
        GIT_TAG v3.15.0
    )
    FetchContent_MakeAvailable(entt)
endif()

if(NOT TARGET EnTT::EnTT AND TARGET entt)
    add_library(EnTT::EnTT ALIAS entt)
endif()

if(SCAFFOLD_BUILD_TESTS)
    find_package(GTest CONFIG QUIET)
    if(NOT TARGET GTest::gtest_main AND SCAFFOLD_FETCH_DEPS)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    if(NOT TARGET GTest::gtest_main)
        message(FATAL_ERROR "GoogleTest is required when SCAFFOLD_BUILD_TESTS=ON. Install GTest or configure with SCAFFOLD_FETCH_DEPS=ON.")
    endif()
endif()

if(SCAFFOLD_BUILD_CLIENT)
    find_package(raylib QUIET)
    if(NOT TARGET raylib AND SCAFFOLD_FETCH_DEPS AND SCAFFOLD_FETCH_RAYLIB)
        FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG 5.5
        )
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(raylib)
    endif()

    if(NOT TARGET raylib)
        message(FATAL_ERROR "Raylib is required when SCAFFOLD_BUILD_CLIENT=ON. Install raylib or configure with SCAFFOLD_FETCH_DEPS=ON and SCAFFOLD_FETCH_RAYLIB=ON.")
    endif()
endif()

if(SCAFFOLD_ENABLE_RTC_TRANSPORT)
    find_package(LibDataChannel CONFIG QUIET)
endif()
