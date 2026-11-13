
cmake_minimum_required(VERSION 3.20)

project(RooftopSolarShadowAnalyzer
    VERSION 1.0
    LANGUAGES C CXX
)

# ---------------- C++ Standard ----------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------- FetchContent ----------------

include(FetchContent)

# ---------------- GLFW ----------------

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw)

# ---------------- GLM ----------------

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glm)

# ---------------- GLAD 2 ----------------

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glad)

add_subdirectory(
    "${glad_SOURCE_DIR}/cmake"
    glad_cmake
)

glad_add_library(
    glad_gl_core_33
    STATIC
    REPRODUCIBLE
    LOADER
    API gl:core=3.3
)

# ---------------- Assimp ----------------

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(assimp)

# ---------------- Dear ImGui ----------------

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(imgui)
# ---------------- JSON Library ----------------

find_package(nlohmann_json REQUIRED)

# ---------------- CURL Library ----------------

find_package(CURL REQUIRED)
# ---------------- Application ----------------

add_executable(RooftopSolarShadowAnalyzer
    src/main.cpp
    src/Window.cpp
    src/Camera.cpp
    src/Shader.cpp
    src/Mesh.cpp
    src/Model.cpp
    src/SunCalculator.cpp
    src/ShadowMap.cpp
    src/HeatmapBaker.cpp
    src/SolarPanel.cpp
    src/WeatherClient.cpp
)

# ---------------- ImGui Source Files ----------------

target_sources(
    RooftopSolarShadowAnalyzer
    PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# ---------------- Include Directories ----------------

target_include_directories(
    RooftopSolarShadowAnalyzer
    PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---------------- Compile Definitions ----------------

target_compile_definitions(
    RooftopSolarShadowAnalyzer
    PRIVATE
    PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

# ---------------- Link Libraries ----------------

target_link_libraries(
    RooftopSolarShadowAnalyzer
    PRIVATE
    glfw
    glm::glm
    glad_gl_core_33
    assimp
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

# ---------------- macOS Frameworks ----------------

if(APPLE)
    target_link_libraries(
        RooftopSolarShadowAnalyzer
        PRIVATE
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
    )
endif()

# ---------------- Compiler Warnings ----------------

if(MSVC)
    target_compile_options(
        RooftopSolarShadowAnalyzer
        PRIVATE
        /W4
    )
else()
    target_compile_options(
        RooftopSolarShadowAnalyzer
        PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()
