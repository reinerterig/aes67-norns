cmake_minimum_required(VERSION 3.10)
project(aes67_bridge)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find JACK package
find_package(PkgConfig REQUIRED)
pkg_check_modules(JACK REQUIRED jack)

# Add include directories
include_directories(${JACK_INCLUDE_DIRS})

# Source files
set(SOURCES
    src/main.cpp
    src/AES67Bridge.cpp
    src/NetworkManager.cpp
    src/RTPHandler.cpp
    src/PTPSync.cpp
    src/AudioConverter.cpp
)

# Create executable
add_executable(aes67_bridge ${SOURCES})

# Link libraries
target_link_libraries(aes67_bridge ${JACK_LIBRARIES} pthread)

# Add compiler flags
target_compile_options(aes67_bridge PRIVATE -Wall -Wextra)

# Install target
install(TARGETS aes67_bridge DESTINATION bin)
