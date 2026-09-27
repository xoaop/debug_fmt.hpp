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

# Smoke test — built only when debug_fmt is the top-level project, so consumers
# that pull it in via add_subdirectory (e.g. as a submodule) don't build it.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(debug_fmt_test test.cpp)
    target_link_libraries(debug_fmt_test PRIVATE debug_fmt::debug_fmt)
    if(WIN32)
        # std::print's terminal helpers live in libstdc++exp on MinGW; -static
        # avoids a mismatched libstdc++ DLL; -mconsole keeps stdout visible.
        target_link_libraries(debug_fmt_test PRIVATE stdc++exp)
        target_link_options(debug_fmt_test PRIVATE -mconsole -static)
    endif()
endif()
