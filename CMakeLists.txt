cmake_minimum_required(VERSION 3.20)
project(event_horizon CXX)

# c++17 everywhere, no compiler specific extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# windows compilers only give us M_PI with this
add_compile_definitions(_USE_MATH_DEFINES)

# default to an optimized build so the tracer renders at real speed
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# the cpu reference tracer, shared by the tests and the render tool
add_library(core core/tracer.cpp core/render.cpp core/disk.cpp)
target_include_directories(core PUBLIC core)
target_compile_options(core PRIVATE -Wall -Wextra)

# native test runner with all the physics tests
enable_testing()
add_executable(tests tests/test_main.cpp tests/test_tracer.cpp tests/test_render.cpp tests/test_disk.cpp)
target_include_directories(tests SYSTEM PRIVATE third_party/doctest)
target_link_libraries(tests PRIVATE core)
target_compile_options(tests PRIVATE -Wall -Wextra)
add_test(NAME tests COMMAND tests)

# command line tool that renders reference images into results/
add_executable(render tools/render.cpp)
target_include_directories(render SYSTEM PRIVATE third_party/stb)
target_link_libraries(render PRIVATE core)
target_compile_options(render PRIVATE -Wall -Wextra)

# measures how much brighter the approaching side of the disk is, numbers go in results/
add_executable(beaming_ratio tools/beaming_ratio.cpp)
target_link_libraries(beaming_ratio PRIVATE core)
target_compile_options(beaming_ratio PRIVATE -Wall -Wextra)
