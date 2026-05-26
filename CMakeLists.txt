cmake_minimum_required(VERSION 3.20)

project(
    parallel_decoding_showcase
    VERSION 0.1.0
    DESCRIPTION "Lock-free parallel decoding benchmark for distributed storage"
    LANGUAGES CXX
)

option(PDE_ENABLE_OPENMP "Enable OpenMP row-parallel execution" ON)
option(PDE_NATIVE_ARCH "Tune release builds for the host CPU" OFF)
option(PDE_BUILD_BENCHMARKS "Build benchmark executables" ON)
option(PDE_BUILD_TESTS "Build correctness tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(pde_codec
    src/codec.cpp
)

target_include_directories(pde_codec
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(pde_codec PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(pde_codec PRIVATE /W4 /permissive-)
else()
    target_compile_options(pde_codec PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(PDE_NATIVE_ARCH)
    if(MSVC)
        target_compile_options(pde_codec PRIVATE /arch:AVX2)
    else()
        target_compile_options(pde_codec PRIVATE -march=native)
    endif()
endif()

if(PDE_ENABLE_OPENMP)
    find_package(OpenMP QUIET)
endif()

if(PDE_ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(pde_codec PUBLIC OpenMP::OpenMP_CXX)
    target_compile_definitions(pde_codec PUBLIC PDE_HAS_OPENMP=1)
else()
    target_compile_definitions(pde_codec PUBLIC PDE_HAS_OPENMP=0)
endif()

if(PDE_BUILD_BENCHMARKS)
    add_executable(pde_bench
        benchmarks/benchmark_main.cpp
    )
    target_link_libraries(pde_bench PRIVATE pde_codec)
endif()

if(PDE_BUILD_TESTS)
    enable_testing()

    add_executable(pde_codec_tests
        tests/codec_tests.cpp
    )
    target_link_libraries(pde_codec_tests PRIVATE pde_codec)

    add_test(NAME pde_codec_tests COMMAND pde_codec_tests)
endif()
