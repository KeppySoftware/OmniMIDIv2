cmake_minimum_required(VERSION 3.26)
project(OmniMIDIv2)

set(CMAKE_CXX_STANDARD 20)

file(GLOB_RECURSE srcs CONFIGURE_DEPENDS 
        *.h
        *.cpp 
        *.hpp)

set(ROOT ${CMAKE_CURRENT_LIST_DIR})
include_directories(${ROOT})
add_subdirectory(inc/nlohmann)

add_library(OmniMIDI SHARED ${srcs})
target_link_libraries(OmniMIDI PRIVATE stdc++)
