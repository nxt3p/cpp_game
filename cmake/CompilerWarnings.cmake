function(engine_apply_warnings target_name)
    target_compile_options(${target_name} PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow
    )
endfunction()

function(engine_apply_debug_sanitizers target_name)
    if(ENGINE_WINDOWS_CROSS OR CMAKE_SYSTEM_NAME STREQUAL "Windows" OR CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        return()
    endif()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(${target_name} PRIVATE -O0 -g)
        target_compile_options(${target_name} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
        target_link_options(${target_name} PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endfunction()

function(engine_apply_release_opts target_name)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(${target_name} PRIVATE -O3)
    endif()
endfunction()
