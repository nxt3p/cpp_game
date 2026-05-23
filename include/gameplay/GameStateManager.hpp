#pragma once

#include "gameplay/GameTypes.hpp"

#include <stdexcept>
#include <string>

namespace gameplay {

class GameStateManager {
public:
    [[nodiscard]] GameState currentState() const noexcept { return currentState_; }

    void transitionTo(GameState nextState);
    void openCharacterMenu();
    void closeCharacterMenu();
    void enterTrading();
    void leaveTrading();

    void forceState(GameState state) noexcept;

    [[nodiscard]] bool isPausedForUi() const noexcept;
    [[nodiscard]] std::string stateLabel() const;

private:
    void assertTransitionAllowed(GameState nextState) const;

    GameState currentState_{GameState::TOWN};
    GameState previousGameplayState_{GameState::TOWN};
};

} // namespace gameplay
