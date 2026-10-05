cmake_minimum_required(VERSION 3.16)
project(intel_rdrand_prng LANGUAGES CXX)

add_library(intel_rdrand_prng src/rdrand_prng.cpp)
target_include_directories(intel_rdrand_prng PUBLIC include)
target_compile_features(intel_rdrand_prng PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(intel_rdrand_prng PRIVATE /W4 /permissive-)
else()
    target_compile_options(intel_rdrand_prng PRIVATE -Wall -Wextra -Wpedantic)
endif()

option(RDRAND_PRNG_BUILD_TESTS "Build unit tests" ON)
if(RDRAND_PRNG_BUILD_TESTS)
    enable_testing()
    add_executable(rdrand_prng_tests tests/test_prng.cpp)
    target_link_libraries(rdrand_prng_tests PRIVATE intel_rdrand_prng)
    add_test(NAME rdrand_prng_tests COMMAND rdrand_prng_tests)
endif()

option(RDRAND_PRNG_BUILD_APP "Build the local web dashboard" ON)
if(RDRAND_PRNG_BUILD_APP)
    add_executable(rdrand_dashboard app/main.cpp)
    target_link_libraries(rdrand_dashboard PRIVATE intel_rdrand_prng)
    if(WIN32)
        target_link_libraries(rdrand_dashboard PRIVATE ws2_32)
    endif()

    add_custom_command(TARGET rdrand_dashboard POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/web
                $<TARGET_FILE_DIR:rdrand_dashboard>/web
        COMMENT "Copying dashboard assets")
endif()
