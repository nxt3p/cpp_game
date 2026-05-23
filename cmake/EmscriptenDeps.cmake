include(FetchContent)

set(GLM_VERSION "1.0.1")
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG ${GLM_VERSION}
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glm)

add_library(EngineEmscriptenDeps INTERFACE)
target_link_libraries(EngineEmscriptenDeps INTERFACE glm::glm)
target_compile_definitions(EngineEmscriptenDeps INTERFACE __EMSCRIPTEN__)

function(engine_link_graphics target_name)
    target_link_libraries(${target_name} PUBLIC EngineEmscriptenDeps)
endfunction()
