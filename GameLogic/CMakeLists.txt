add_library(GameLogic STATIC
    src/Map.cpp
    src/InterestManagement.cpp
    src/LagCompensation.cpp
    src/SnapshotBaselineCache.cpp
    src/NetworkEvents.cpp
    src/ReplicationPlanner.cpp
    src/Serialization.cpp
    src/Simulation.cpp
    src/SimulationCombat.cpp
    src/SimulationMovement.cpp
    src/TimeSync.cpp
    src/Weapons.cpp
)

target_include_directories(GameLogic
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(NOT TARGET EnTT::EnTT)
    message(FATAL_ERROR "EnTT was not found. Install EnTT or configure with SCAFFOLD_FETCH_DEPS=ON.")
endif()

target_link_libraries(GameLogic PUBLIC EnTT::EnTT)
scaffold_apply_common_options(GameLogic)
