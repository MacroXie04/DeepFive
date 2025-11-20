cmake_minimum_required(VERSION 3.10)
project(DeepFive)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find FLTK
find_package(FLTK REQUIRED)

# Find OpenGL
find_package(OpenGL REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${FLTK_INCLUDE_DIRS})
include_directories(${OPENGL_INCLUDE_DIR})

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Executable
add_executable(DeepFive ${SOURCES})

# Link libraries
target_link_libraries(DeepFive ${FLTK_LIBRARIES} ${OPENGL_LIBRARIES})

# On MacOS, FLTK might need extra flags
if(APPLE)
    target_link_libraries(DeepFive "-framework Cocoa")
endif()

