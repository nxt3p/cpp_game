#include "game/GameApplication.hpp"
#include "game/GameDebug.hpp"
#include "Window.hpp"

#include <iostream>
#include <string>

namespace {

std::string assetsRoot() {
#ifdef ENGINE_ASSETS_DIR
    return ENGINE_ASSETS_DIR;
#elif defined(ENGINE_ASSETS_RELATIVE)
    return ENGINE_ASSETS_RELATIVE;
#else
    return "assets";
#endif
}

} // namespace

int main() {
    try {
        game::logBanner();
        game::logInfo("Creating window (1280x720)...");
        engine::Window window(1280, 720, "GameEngine");
        game::logInfo("Window and OpenGL context ready.");

        game::logInfo("Constructing GameApplication...");
        game::GameApplication application(window, assetsRoot());
        application.run();
        game::logInfo("Application exited cleanly.");
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal engine error: " << ex.what() << '\n';
        return 1;
    }
}
