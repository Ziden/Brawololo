add_library(Networking STATIC
    src/KcpRtcTransport.cpp
    src/LoopbackTransport.cpp
)

target_include_directories(Networking
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(Networking PUBLIC GameLogic)
scaffold_apply_common_options(Networking)

if(SCAFFOLD_ENABLE_RTC_TRANSPORT AND TARGET LibDataChannel::LibDataChannel)
    target_link_libraries(Networking PRIVATE LibDataChannel::LibDataChannel)
    target_compile_definitions(Networking PRIVATE SCAFFOLD_ENABLE_RTC_TRANSPORT=1)
endif()

