add_library(ClientCore STATIC
    src/ClientApplication.cpp
    src/ClientEventLog.cpp
    src/ClientProtocolPump.cpp
    src/ClientRuntime.cpp
    src/ClientViewModel.cpp
    src/FixedStepClock.cpp
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
    src/main.cpp
    src/RaylibClientHost.cpp
    src/RaylibDebugOverlay.cpp
    src/RaylibGameView.cpp
    src/RaylibSceneRenderer.cpp
)

target_link_libraries(Client PRIVATE ClientCore raylib)
if(TARGET ClientInProcess)
    target_link_libraries(Client PRIVATE ClientInProcess)
endif()

scaffold_apply_common_options(Client)

if(TARGET ServerCore)
    add_executable(SingleProcess
        src/single_process_main.cpp
    )

    target_link_libraries(SingleProcess PRIVATE ClientInProcess)
    scaffold_apply_common_options(SingleProcess)
endif()
