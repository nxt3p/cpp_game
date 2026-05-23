# Cross-compile target: Windows 32-bit x86 (PE32) using MinGW-w64 from Linux/WSL.
# Runs on Windows 11 via WOW64. Prefer mingw-x86_64-w64.cmake for native 64-bit builds.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(CMAKE_C_COMPILER i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER i686-w64-mingw32-g++)
set(CMAKE_RC_COMPILER i686-w64-mingw32-windres)
set(CMAKE_AR i686-w64-mingw32-ar)
set(CMAKE_RANLIB i686-w64-mingw32-ranlib)
set(CMAKE_STRIP i686-w64-mingw32-strip)

set(CMAKE_FIND_ROOT_PATH /usr/i686-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENGINE_WINDOWS_CROSS TRUE CACHE BOOL "MinGW Windows cross build" FORCE)
set(ENGINE_WINDOWS_ARCH "i686" CACHE STRING "Windows MinGW architecture" FORCE)
