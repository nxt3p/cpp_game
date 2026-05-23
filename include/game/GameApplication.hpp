#pragma once

#include "game/AppFlow.hpp"
#include "gameplay/GameTypes.hpp"
#include "Window.hpp"

#include <string>

namespace game {

class GameApplication {
public:
    GameApplication(engine::Window& window, std::string assetsRoot);
    ~GameApplication();

    GameApplication(const GameApplication&) = delete;
    GameApplication& operator=(const GameApplication&) = delete;

    void run();

    /// Single update + render pass (used by boot/integration tests).
    void tickOneFrame(float deltaSeconds = 1.0F / 60.0F);

    // --- Synthetic input & state queries (integration tests) ---
    void simulateMouseMove(float screenX, float screenY);
    void simulateMouseClick(float screenX, float screenY);
    void simulateKeyPress(int glfwKey);

    [[nodiscard]] AppScreen currentScreen() const;
    [[nodiscard]] gameplay::WorldZone activeWorldZone() const;
    [[nodiscard]] bool isGamePaused() const;
    [[nodiscard]] int characterLevel() const;
    [[nodiscard]] int characterExperience() const;
    [[nodiscard]] bool projectWorldToScreen(
        float worldX,
        float worldY,
        float worldZ,
        float& screenX,
        float& screenY) const;

    /// Projects the nearest living attackable mob to screen space (integration tests).
    [[nodiscard]] bool tryGetNearestAttackableMobScreenPosition(float& screenX, float& screenY);

    /// Locks combat onto the nearest mob and moves the player into melee range (integration tests).
    [[nodiscard]] bool engageNearestMobForTest();

    [[nodiscard]] int runDepth() const;
    [[nodiscard]] int lootCoinPool() const;
    [[nodiscard]] int playerInventoryUsedSlots() const;

    /// Sets player XZ and syncs zone logic (integration tests only).
    void setPlayerWorldPositionForTest(float worldX, float worldZ);

    /// Runs a timed plains benchmark; returns median milliseconds per frame.
    [[nodiscard]] double runPlainsFrameBenchmarkForTest(int warmupFrames, int measureFrames);

    [[nodiscard]] double lastBenchmarkMedianFrameMs() const;

private:
    struct Impl;
    Impl* impl_{nullptr};
};

} // namespace game
