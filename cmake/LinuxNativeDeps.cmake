find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
pkg_check_modules(GLM REQUIRED IMPORTED_TARGET glm)

function(engine_link_graphics target_name)
    target_link_libraries(${target_name} PRIVATE
        OpenGL::GL
        PkgConfig::GLFW3
        PkgConfig::GLEW
        PkgConfig::GLM
    )
endfunction()
