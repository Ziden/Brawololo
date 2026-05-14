add_library(ClientCore STATIC
    src/ClientApplication.cpp
    src/ClientEventLog.cpp
    src/ClientProtocolPump.cpp
    src/ClientRuntime.cpp
    src/ClientVisualEffectLog.cpp
    src/ClientViewModel.cpp
    src/FixedStepClock.cpp
    src/LocalPreviewClientSession.cpp
    src/PendingInputHistory.cpp
    src/PresentationInterpolator.cpp
    src/RemoteClientSession.cpp
)

target_include_directories(ClientCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(ClientCore PUBLIC GameLogic Networking)
scaffold_apply_common_options(ClientCore)

if(TARGET ServerCore)
    add_library(ClientInProcess STATIC
        src/InProcessClientSession.cpp
    )

    target_include_directories(ClientInProcess
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )

    target_link_libraries(ClientInProcess PUBLIC ClientCore ServerCore)
    scaffold_apply_common_options(ClientInProcess)
endif()

add_executable(Client
    src/RaylibClientHost.cpp
    src/RaylibDebugOverlay.cpp
    src/RaylibGameView.cpp
    src/RaylibSceneRenderer.cpp
)

if(EMSCRIPTEN)
    target_sources(Client PRIVATE src/browser_main.cpp)
else()
    target_sources(Client PRIVATE src/main.cpp)
endif()

target_link_libraries(Client PRIVATE ClientCore raylib)
if(TARGET ClientInProcess)
    target_link_libraries(Client PRIVATE ClientInProcess)
endif()

scaffold_apply_common_options(Client)

if(EMSCRIPTEN)
    target_compile_definitions(Client PRIVATE SCAFFOLD_BROWSER_CLIENT=1)
    target_link_options(Client PRIVATE
        "-sUSE_GLFW=3"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sASSERTIONS=1"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
    set_target_properties(Client PROPERTIES SUFFIX ".html")
endif()

if(TARGET ServerCore)
    add_executable(SingleProcess
        src/single_process_main.cpp
    )

    target_link_libraries(SingleProcess PRIVATE ClientInProcess)
    scaffold_apply_common_options(SingleProcess)

    add_executable(TwoClientSmoke
        src/two_client_smoke_main.cpp
    )

    target_link_libraries(TwoClientSmoke PRIVATE ClientCore ServerCore)
    scaffold_apply_common_options(TwoClientSmoke)
endif()
