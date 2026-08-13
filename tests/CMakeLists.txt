cmake_minimum_required(VERSION 3.15)
project(runtime_tests VERSION 0.11.5)

add_executable(runtime_tests RendererTests.cpp)

target_compile_features(runtime_tests PRIVATE cxx_std_17)

target_link_libraries(runtime_tests PRIVATE elem::runtime)

# This test harness relies on assert() actually firing to detect failures (see
# main()'s dead `failures` counter otherwise), so force assertions to stay
# active even in Release builds (which define NDEBUG by default).
target_compile_options(runtime_tests PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-UNDEBUG>)

enable_testing()
add_test(NAME runtime_tests COMMAND runtime_tests)
