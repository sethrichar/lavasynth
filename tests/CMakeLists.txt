FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(rotor_tests
    OscillatorTests.cpp
    FilterTests.cpp
    EnvelopeTests.cpp
    VoiceTests.cpp
    AllocatorTests.cpp
    GlideTests.cpp)

target_link_libraries(rotor_tests PRIVATE rotor_dsp Catch2::Catch2WithMain)

include(CTest)
include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
catch_discover_tests(rotor_tests)
