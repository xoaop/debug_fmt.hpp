cmake_minimum_required(VERSION 3.20)
project(debug_fmt LANGUAGES CXX)

# Header-only INTERFACE library. Consume it with:
#   add_subdirectory(path/to/debug_fmt)
#   target_link_libraries(your_target PRIVATE debug_fmt::debug_fmt)
#
# The include directory and (on GCC) the -freflection flag propagate to any
# target that links it, so consumers don't need to know the requirement.
add_library(debug_fmt INTERFACE)
add_library(debug_fmt::debug_fmt ALIAS debug_fmt)

target_include_directories(debug_fmt INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(debug_fmt INTERFACE cxx_std_26)

# debug_fmt.hpp uses C++26 static reflection (P2996: <meta>, ^^, template for),
# available today only in GCC (>=16) via -freflection.
target_compile_options(debug_fmt INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-freflection>)
