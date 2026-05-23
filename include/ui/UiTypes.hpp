#pragma once

#include <vector>

namespace ui {

struct Vec2 {
    float x{0.0F};
    float y{0.0F};
};

struct Rect2D {
    float x{0.0F};
    float y{0.0F};
    float width{0.0F};
    float height{0.0F};
};

struct CharacterScreenData {
    int level{1};
    int experience{0};
    int experienceToNextLevel{100};
    int carriedSouls{0};
    int statUpgradesPurchased{0};
    float soulGainMultiplier{1.0F};
    int strength{10};
    int dexterity{10};
    int vitality{10};
    int unspentPoints{0};
    bool visible{false};
};

struct InventoryOverlayState {
    bool visible{false};
    int columns{6};
    int rows{4};
    std::vector<bool> slotVisible;
};

} // namespace ui
