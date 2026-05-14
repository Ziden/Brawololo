include(GoogleTest)

add_executable(ScaffoldTests
    GameLogicTests.cpp
    ClientTests.cpp
    NetworkingTests.cpp
    ServerTests.cpp
)

target_link_libraries(ScaffoldTests
    PRIVATE
        GameLogic
        Networking
        ServerCore
        ClientCore
        ClientInProcess
        GTest::gtest_main
)

scaffold_apply_common_options(ScaffoldTests)

gtest_discover_tests(ScaffoldTests)

if(TARGET TwoClientSmoke)
    add_test(NAME Smoke.TwoClient COMMAND TwoClientSmoke)
    add_test(NAME Smoke.MinimalPlayable COMMAND TwoClientSmoke)
endif()

if(TARGET RtcSmoke)
    add_test(NAME Smoke.RtcTransport COMMAND RtcSmoke)
endif()
