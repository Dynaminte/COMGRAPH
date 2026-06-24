cmake_minimum_required(VERSION 3.10)
project(TankDefender)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cpp
    src/Game.cpp
    src/Player.cpp
    src/Enemy.cpp
    src/Bullet.cpp
    src/Turret.cpp
    src/Renderer.cpp
)

# Executable
add_executable(TankDefender ${SOURCES})

# Find packages
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLM REQUIRED)

# Link libraries
target_link_libraries(TankDefender
    GLEW::GLEW
    glfw
    GLM::GLM
)

# Include directories
target_include_directories(TankDefender PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# For macOS
if(APPLE)
    target_link_libraries(TankDefender
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
    )
endif()

# Install
install(TARGETS TankDefender DESTINATION bin)
