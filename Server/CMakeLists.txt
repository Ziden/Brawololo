add_library(ServerCore STATIC
    src/ServerApplication.cpp
    src/ServerNetworkHost.cpp
    src/ServerRuntime.cpp
    src/SimulationOnlyClientDriver.cpp
)

target_include_directories(ServerCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(ServerCore PUBLIC GameLogic Networking)
scaffold_apply_common_options(ServerCore)

add_executable(Server
    src/main.cpp
)

target_link_libraries(Server PRIVATE ServerCore)
scaffold_apply_common_options(Server)
