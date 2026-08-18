cmake_minimum_required(VERSION 3.20)
project(native_renderer_tests VERSION 0.11.5)

add_executable(native-renderer-tests EXCLUDE_FROM_ALL NativeRendererTests.cpp)

target_link_libraries(native-renderer-tests PRIVATE elem::runtime)
