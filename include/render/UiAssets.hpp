#pragma once

#include "render/Texture.hpp"

#include <array>
#include <string>

namespace render {

/// Loaded UI textures from assets/textures/ui.
class UiAssets {
public:
    [[nodiscard]] bool load(const std::string& uiDirectory);
    [[nodiscard]] bool isLoaded() const noexcept { return loaded_; }

    [[nodiscard]] const Texture& barFrame() const noexcept { return barFrame_; }
    [[nodiscard]] const Texture& hpBarFill() const noexcept { return hpBarFill_; }
    [[nodiscard]] const Texture& hudCorner() const noexcept { return hudCorner_; }
    [[nodiscard]] const Texture& heartRed() const noexcept { return heartRed_; }
    [[nodiscard]] const Texture& heartYellow() const noexcept { return heartYellow_; }
    [[nodiscard]] const Texture& inventoryPanelSlice() const noexcept { return inventoryPanelSlice_; }
    [[nodiscard]] const Texture& inventorySlot() const noexcept { return inventorySlot_; }
    [[nodiscard]] const Texture& inventorySlotHover() const noexcept { return inventorySlotHover_; }
    [[nodiscard]] const Texture& settingsBarTrack() const noexcept { return settingsBarTrack_; }
    [[nodiscard]] const Texture& settingsBarFill() const noexcept { return settingsBarFill_; }
    [[nodiscard]] const Texture& settingsBarKnob() const noexcept { return settingsBarKnob_; }
    [[nodiscard]] const Texture& settingsCross() const noexcept { return settingsCross_; }
    [[nodiscard]] const Texture& portraitSheet() const noexcept { return portraitSheet_; }

    [[nodiscard]] const std::array<Texture, 5>& xpHeartFrames() const noexcept { return xpHeartFrames_; }
    [[nodiscard]] const std::array<Texture, 5>& hpHeartFrames() const noexcept { return hpHeartFrames_; }

private:
    bool loaded_{false};
    Texture barFrame_{};
    Texture hpBarFill_{};
    Texture hudCorner_{};
    Texture heartRed_{};
    Texture heartYellow_{};
    Texture inventoryPanelSlice_{};
    Texture inventorySlot_{};
    Texture inventorySlotHover_{};
    Texture settingsBarTrack_{};
    Texture settingsBarFill_{};
    Texture settingsBarKnob_{};
    Texture settingsCross_{};
    Texture portraitSheet_{};
    std::array<Texture, 5> xpHeartFrames_{};
    std::array<Texture, 5> hpHeartFrames_{};
};

} // namespace render
