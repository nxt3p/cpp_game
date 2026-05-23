#include "game/SaveGamePlatform.hpp"

#include <fstream>
#include <sstream>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <cstdlib>
#endif

namespace game::save_platform {

namespace {

#if defined(__EMSCRIPTEN__)

constexpr const char* kWebLocalStorageKey = "cppGame_save_v1";

EM_JS(int, js_local_storage_has, (const char* key), {
    try {
        return localStorage.getItem(UTF8ToString(key)) ? 1 : 0;
    } catch (e) {
        return 0;
    }
});

EM_JS(int, js_local_storage_set, (const char* key, const char* value), {
    try {
        localStorage.setItem(UTF8ToString(key), UTF8ToString(value));
        return 1;
    } catch (e) {
        return 0;
    }
});

EM_JS(char*, js_local_storage_get_copy, (const char* key), {
    try {
        const value = localStorage.getItem(UTF8ToString(key));
        if (!value) {
            return 0;
        }
        const length = lengthBytesUTF8(value) + 1;
        const pointer = _malloc(length);
        if (!pointer) {
            return 0;
        }
        stringToUTF8(value, pointer, length);
        return pointer;
    } catch (e) {
        return 0;
    }
});

[[nodiscard]] const char* webStorageKey(const std::filesystem::path& /*path*/) {
    return kWebLocalStorageKey;
}

#else

[[nodiscard]] SaveGameResult ensureParentDirectory(const std::filesystem::path& path) {
    SaveGameResult result{};
    std::error_code errorCode;
    const std::filesystem::path directory = path.parent_path();
    if (directory.empty()) {
        result.success = true;
        return result;
    }

    std::filesystem::create_directories(directory, errorCode);
    if (errorCode) {
        result.message = "Failed to create save directory: " + errorCode.message();
        return result;
    }

    result.success = true;
    return result;
}

#endif

} // namespace

bool payloadExists(const std::filesystem::path& path) {
#if defined(__EMSCRIPTEN__)
    (void)path;
    return js_local_storage_has(webStorageKey(path)) != 0;
#else
    return std::filesystem::exists(path);
#endif
}

SaveGameResult writePayload(const std::filesystem::path& path, const std::string& payload) {
#if defined(__EMSCRIPTEN__)
    (void)path;
    SaveGameResult result{};
    if (js_local_storage_set(webStorageKey(path), payload.c_str()) == 0) {
        result.message = "Failed to write save to browser localStorage.";
        return result;
    }

    result.success = true;
    result.message = "Game saved to browser localStorage.";
    return result;
#else
    SaveGameResult result = ensureParentDirectory(path);
    if (!result.success && !result.message.empty()) {
        return result;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        result.success = false;
        result.message = "Failed to open save file for writing.";
        return result;
    }

    output << payload;
    if (!output.good()) {
        result.success = false;
        result.message = "Failed while writing save file.";
        return result;
    }

    result.success = true;
    result.message = "Game saved.";
    return result;
#endif
}

SaveGameResult readPayload(const std::filesystem::path& path, std::string& outPayload) {
#if defined(__EMSCRIPTEN__)
    (void)path;
    SaveGameResult result{};
    char* copied = js_local_storage_get_copy(webStorageKey(path));
    if (copied == nullptr) {
        result.message = "Save file not found in browser localStorage.";
        return result;
    }

    outPayload.assign(copied);
    std::free(copied);

    result.success = true;
    return result;
#else
    SaveGameResult result{};
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        result.message = "Save file not found.";
        return result;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    outPayload = buffer.str();

    result.success = true;
    return result;
#endif
}

std::string storageDescription(const std::filesystem::path& path) {
#if defined(__EMSCRIPTEN__)
    (void)path;
    return std::string("localStorage:") + kWebLocalStorageKey;
#else
    return path.string();
#endif
}

} // namespace game::save_platform
