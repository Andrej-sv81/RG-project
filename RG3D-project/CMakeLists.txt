cmake_minimum_required(VERSION 3.10)
project(RG3D-project)

set(CMAKE_CXX_STANDARD 17)

# Add include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/headers
    /usr/include/freetype2
    /usr/include
)

# Source files
file(GLOB_RECURSE SOURCES
    "RG3D-project.cpp"
)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(Freetype REQUIRED)
find_package(assimp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

include_directories(${GLFW_INCLUDE_DIRS})
include_directories(${GLEW_INCLUDE_DIRS})
include_directories(${ASSIMP_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
    ${OPENGL_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${GLFW_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${ASSIMP_LIBRARIES}
)