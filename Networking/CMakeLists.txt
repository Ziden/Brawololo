add_library(Networking STATIC
    src/KcpRtcTransport.cpp
    src/KcpRtcDataChannelPump.cpp
    src/KcpRtcSignalingPump.cpp
    src/LoopbackTransport.cpp
    src/MultiClientLoopbackTransport.cpp
    src/RtcDataChannel.cpp
    src/RtcSignaling.cpp
    src/RtcSignalingClient.cpp
    src/TransportPacketCodec.cpp
    src/TransportFactory.cpp
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
