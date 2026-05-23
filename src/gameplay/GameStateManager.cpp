#include "gameplay/GameStateManager.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace gameplay {

namespace {

const char* toString(GameState state) {
    switch (state) {
    case GameState::TOWN:
        return "TOWN";
    case GameState::PLAINS:
        return "PLAINS";
    case GameState::TRADING:
        return "TRADING";
    case GameState::CHARACTER_MENU:
        return "CHARACTER_MENU";
    }
    return "UNKNOWN";
}

} // namespace

void GameStateManager::transitionTo(GameState nextState) {
    assertTransitionAllowed(nextState);

    if (nextState == GameState::CHARACTER_MENU) {
        previousGameplayState_ = currentState_;
    } else if (currentState_ != GameState::CHARACTER_MENU) {
        previousGameplayState_ = nextState;
    }

    currentState_ = nextState;
}

void GameStateManager::openCharacterMenu() {
    if (currentState_ == GameState::TRADING) {
        throw std::logic_error("Cannot open character menu while trading");
    }
    previousGameplayState_ = currentState_;
    currentState_ = GameState::CHARACTER_MENU;
}

void GameStateManager::closeCharacterMenu() {
    if (currentState_ != GameState::CHARACTER_MENU) {
        return;
    }
    currentState_ = previousGameplayState_;
}

void GameStateManager::enterTrading() {
    if (currentState_ != GameState::TOWN) {
        throw std::logic_error("Trading is only available in town");
    }
    previousGameplayState_ = currentState_;
    currentState_ = GameState::TRADING;
}

void GameStateManager::leaveTrading() {
    if (currentState_ != GameState::TRADING) {
        return;
    }
    currentState_ = GameState::TOWN;
}

void GameStateManager::forceState(const GameState state) noexcept {
    currentState_ = state;
    if (state == GameState::TOWN || state == GameState::PLAINS) {
        previousGameplayState_ = state;
    }
}

bool GameStateManager::isPausedForUi() const noexcept {
    return currentState_ == GameState::CHARACTER_MENU || currentState_ == GameState::TRADING;
}

std::string GameStateManager::stateLabel() const {
    return toString(currentState_);
}

void GameStateManager::assertTransitionAllowed(GameState nextState) const {
    static const std::unordered_map<GameState, std::vector<GameState>> allowed = {
        {GameState::TOWN, {GameState::PLAINS, GameState::TRADING, GameState::CHARACTER_MENU}},
        {GameState::PLAINS, {GameState::TOWN, GameState::CHARACTER_MENU}},
        {GameState::TRADING, {GameState::TOWN}},
        {GameState::CHARACTER_MENU, {GameState::TOWN, GameState::PLAINS}}};

    if (nextState == currentState_) {
        return;
    }

    const auto rules = allowed.find(currentState_);
    if (rules == allowed.end()) {
        throw std::logic_error("No transition rules for current state");
    }

    const auto& destinations = rules->second;
    if (std::find(destinations.begin(), destinations.end(), nextState) == destinations.end()) {
        throw std::logic_error(
            std::string("Illegal transition from ") + stateLabel() + " to " + toString(nextState));
    }
}

} // namespace gameplay
