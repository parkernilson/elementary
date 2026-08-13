cmake_minimum_required(VERSION 3.15)
project(runtime_tests VERSION 0.11.5)

add_executable(runtime_tests RendererTests.cpp)

target_compile_features(runtime_tests PRIVATE cxx_std_17)

target_link_libraries(runtime_tests PRIVATE elem::runtime)
