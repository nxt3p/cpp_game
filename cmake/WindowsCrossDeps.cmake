include(FetchContent)

set(GLM_VERSION "1.0.1")
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG ${GLM_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

set(GLFW_VERSION "3.3.10")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG ${GLFW_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

set(GLEW_CMAKE_VERSION "glew-cmake-2.2.0")
FetchContent_Declare(
    glew_cmake
    GIT_REPOSITORY https://github.com/Perlmint/glew-cmake.git
    GIT_TAG ${GLEW_CMAKE_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glew_cmake)

set(ENGINE_GLEW_TARGET libglew_static)
if(NOT TARGET ${ENGINE_GLEW_TARGET})
    if(TARGET glew_s)
        set(ENGINE_GLEW_TARGET glew_s)
    elseif(TARGET GLEW::GLEW)
        set(ENGINE_GLEW_TARGET GLEW::GLEW)
    else()
        message(FATAL_ERROR "No supported GLEW CMake target found from glew-cmake")
    endif()
endif()

add_library(EngineWindowsDeps INTERFACE)
target_link_libraries(EngineWindowsDeps INTERFACE glm::glm glfw ${ENGINE_GLEW_TARGET})
target_compile_definitions(EngineWindowsDeps INTERFACE GLEW_STATIC)

function(engine_link_graphics target_name)
    target_link_libraries(${target_name} PUBLIC EngineWindowsDeps)
    target_link_libraries(${target_name} PRIVATE opengl32 gdi32 user32 shell32)
endfunction()
