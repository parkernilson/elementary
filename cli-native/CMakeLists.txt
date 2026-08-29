cmake_minimum_required(VERSION 3.15)
project(cli-native VERSION 0.11.0)

add_library(elemcli_native_graph STATIC HelloSine.cpp)

target_include_directories(elemcli_native_graph PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(elemcli_native_graph PUBLIC
  cxx_std_17)

target_link_libraries(elemcli_native_graph PUBLIC
  elem::runtime)

if(MSVC)
  target_compile_options(elemcli_native_graph PRIVATE /W4)
else()
  target_compile_options(elemcli_native_graph PRIVATE -Wall -Wextra)
endif()
