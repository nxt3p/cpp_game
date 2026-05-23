#include "render/UiAssets.hpp"

namespace render {

namespace {

bool loadTexture(Texture& texture, const std::string& path) {
    return texture.loadFromFile(path, true);
}

} // namespace

bool UiAssets::load(const std::string& uiDirectory) {
    loaded_ = false;

    const std::string root =
        uiDirectory.empty() || uiDirectory.back() == '/' ? uiDirectory : uiDirectory + '/';

    if (!loadTexture(barFrame_, root + "Settings.png")) {
        return false;
    }
    if (!loadTexture(hpBarFill_, root + "Health_04_Bar01.png")) {
        return false;
    }
    if (!loadTexture(hudCorner_, root + "Health_04.png")) {
        return false;
    }
    if (!loadTexture(heartRed_, root + "Health_04_Heart_Red.png")) {
        return false;
    }
    if (!loadTexture(heartYellow_, root + "Health_04_Heart_Yellow.png")) {
        return false;
    }
    if (!loadTexture(inventoryPanelSlice_, root + "Inventory_9Slices.png")) {
        return false;
    }
    if (!loadTexture(inventorySlot_, root + "Inventory_Slot_1.png")) {
        return false;
    }
    if (!loadTexture(inventorySlotHover_, root + "Inventory_Slot_2.png")) {
        return false;
    }
    if (!loadTexture(settingsBarTrack_, root + "Settings_Bar01.png")) {
        return false;
    }
    if (!loadTexture(settingsBarFill_, root + "Settings_Bar02.png")) {
        return false;
    }
    if (!loadTexture(settingsBarKnob_, root + "Settings_Bar03.png")) {
        return false;
    }
    if (!loadTexture(settingsCross_, root + "Settings_Cross01.png")) {
        return false;
    }
    if (!loadTexture(portraitSheet_, root + "Health_05_01.png")) {
        return false;
    }

    for (int index = 0; index < 5; ++index) {
        if (!loadTexture(xpHeartFrames_[static_cast<std::size_t>(index)],
                root + "Hearts_Yellow_" + std::to_string(index + 1) + ".png")) {
            return false;
        }
        if (!loadTexture(hpHeartFrames_[static_cast<std::size_t>(index)],
                root + "Hearts_Red_" + std::to_string(index + 1) + ".png")) {
            return false;
        }
    }

    loaded_ = true;
    return true;
}

} // namespace render
