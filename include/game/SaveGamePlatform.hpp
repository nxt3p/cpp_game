#pragma once

#include "game/SaveGame.hpp"

#include <filesystem>
#include <string>

namespace game::save_platform {

[[nodiscard]] bool payloadExists(const std::filesystem::path& path);
[[nodiscard]] SaveGameResult writePayload(
    const std::filesystem::path& path,
    const std::string& payload);
[[nodiscard]] SaveGameResult readPayload(
    const std::filesystem::path& path,
    std::string& outPayload);
[[nodiscard]] std::string storageDescription(const std::filesystem::path& path);

} // namespace game::save_platform
