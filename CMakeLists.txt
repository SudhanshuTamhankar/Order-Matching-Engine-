# --- Project Setup ---
cmake_minimum_required(VERSION 3.10)
project(OrderMatchingEngine VERSION 1.0)

# --- Set C++ Standard ---
# We require C++17 for <filesystem> and other features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Don't use compiler-specific extensions

# --- Find Required Libraries ---

# 1. Find OpenMP
# This is the modern way to add OpenMP to a project
find_package(OpenMP REQUIRED)

# 2. Find Threads (pthreads)
# Required for std::thread, std::mutex, etc.
find_package(Threads REQUIRED)

# --- Project Configuration ---

# Tell CMake where to find our header files
include_directories(include)

# Find all our .cpp source files in the 'src' directory
file(GLOB_RECURSE SOURCES "src/*.cpp")

# --- Build the Executable ---
add_executable(
    matching_engine
    ${SOURCES}
)

# --- Link Libraries to the Executable ---
target_link_libraries(
    matching_engine
    PRIVATE
    Threads::Threads      # Link the pthreads library
    OpenMP::OpenMP_CXX    # Link the OpenMP library
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(matching_engine PRIVATE stdc++fs)
endif()

# --- Optional: Add Optimization ---
# Add -O2 optimization flag for release builds
# (CMake's default "Release" build type does this,
# but it's good to be explicit if needed)
target_compile_options(matching_engine PRIVATE -O2)


# --- Output ---
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Found OpenMP: ${OpenMP_FOUND}")
message(STATUS "Found Threads: ${Threads_FOUND}")
