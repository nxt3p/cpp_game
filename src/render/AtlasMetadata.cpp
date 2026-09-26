#include "render/AtlasMetadata.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace render {

namespace {

[[nodiscard]] std::string readFile(const std::string& path) {
    std::ifstream stream(path);
    if (!stream) {
        return {};
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

[[nodiscard]] std::size_t findKey(const std::string& json, const std::string& key, std::size_t from) {
    const std::string pattern = "\"" + key + "\"";
    return json.find(pattern, from);
}

[[nodiscard]] bool parseIntAfter(const std::string& json, std::size_t keyPos, int& out) {
    const std::size_t colon = json.find(':', keyPos);
    if (colon == std::string::npos) {
        return false;
    }
    std::size_t cursor = colon + 1;
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor])) != 0) {
        ++cursor;
    }
    try {
        std::size_t consumed = 0;
        out = std::stoi(json.substr(cursor), &consumed);
        return consumed > 0;
    } catch (const std::exception&) {
        return false;
    }
}

[[nodiscard]] bool parseFloatAfter(const std::string& json, std::size_t keyPos, float& out) {
    const std::size_t colon = json.find(':', keyPos);
    if (colon == std::string::npos) {
        return false;
    }
    std::size_t cursor = colon + 1;
    while (cursor < json.size() && std::isspace(static_cast<unsigned char>(json[cursor])) != 0) {
        ++cursor;
    }
    try {
        std::size_t consumed = 0;
        out = std::stof(json.substr(cursor), &consumed);
        return consumed > 0;
    } catch (const std::exception&) {
        return false;
    }
}

[[nodiscard]] bool parseStringAfter(const std::string& json, std::size_t keyPos, std::string& out) {
    const std::size_t colon = json.find(':', keyPos);
    if (colon == std::string::npos) {
        return false;
    }
    const std::size_t open = json.find('"', colon + 1);
    if (open == std::string::npos) {
        return false;
    }
    const std::size_t close = json.find('"', open + 1);
    if (close == std::string::npos) {
        return false;
    }
    out = json.substr(open + 1, close - open - 1);
    return !out.empty();
}

[[nodiscard]] bool parseBoolAfter(const std::string& json, std::size_t keyPos, bool& out) {
    const std::size_t colon = json.find(':', keyPos);
    if (colon == std::string::npos) {
        return false;
    }
    if (json.compare(colon + 1, 4, "true") == 0 || json.find("true", colon) < json.find(',', colon)) {
        const std::size_t token = json.find("true", colon);
        const std::size_t comma = json.find(',', colon);
        const std::size_t end = json.find('}', colon);
        const std::size_t limit = std::min(comma, end);
        if (token != std::string::npos && token < limit) {
            out = true;
            return true;
        }
    }
    if (json.find("false", colon) != std::string::npos) {
        const std::size_t token = json.find("false", colon);
        const std::size_t comma = json.find(',', colon);
        const std::size_t end = json.find('}', colon);
        const std::size_t limit = std::min(comma, end);
        if (token < limit) {
            out = false;
            return true;
        }
    }
    return false;
}

template <typename Frame>
void parseObjects(const std::string& json, const std::string& arrayKey, std::vector<Frame>& out, void (*fill)(const std::string&, Frame&)) {
    const std::size_t key = findKey(json, arrayKey, 0);
    if (key == std::string::npos) {
        return;
    }
    const std::size_t begin = json.find('[', key);
    const std::size_t end = json.find(']', begin == std::string::npos ? 0 : begin);
    if (begin == std::string::npos || end == std::string::npos || end < begin) {
        return;
    }
    std::size_t cursor = begin;
    while (cursor < end) {
        const std::size_t open = json.find('{', cursor);
        if (open == std::string::npos || open > end) {
            break;
        }
        const std::size_t close = json.find('}', open);
        if (close == std::string::npos || close > end) {
            break;
        }
        Frame frame{};
        fill(json.substr(open, close - open + 1), frame);
        if (!frame.name.empty()) {
            out.push_back(std::move(frame));
        }
        cursor = close + 1;
    }
}

void fillDirectional(const std::string& object, DirectionalClipInfo& clip) {
    const std::size_t nameKey = findKey(object, "name", 0);
    if (nameKey == std::string::npos || !parseStringAfter(object, nameKey, clip.name)) {
        return;
    }
    int baseRow = 0;
    int frames = 1;
    float fps = 8.0F;
    bool loop = true;
    const std::size_t rowKey = findKey(object, "baseRow", 0);
    const std::size_t frameKey = findKey(object, "frames", 0);
    const std::size_t fpsKey = findKey(object, "fps", 0);
    const std::size_t loopKey = findKey(object, "loop", 0);
    if (rowKey != std::string::npos) {
        static_cast<void>(parseIntAfter(object, rowKey, baseRow));
    }
    if (frameKey != std::string::npos) {
        static_cast<void>(parseIntAfter(object, frameKey, frames));
    }
    if (fpsKey != std::string::npos) {
        static_cast<void>(parseFloatAfter(object, fpsKey, fps));
    }
    if (loopKey != std::string::npos) {
        static_cast<void>(parseBoolAfter(object, loopKey, loop));
    }
    clip.baseRow = baseRow;
    clip.frames = frames;
    clip.fps = fps;
    clip.loop = loop;
}

void fillNamed(const std::string& object, NamedAtlasFrame& frame) {
    const std::size_t nameKey = findKey(object, "name", 0);
    if (nameKey == std::string::npos || !parseStringAfter(object, nameKey, frame.name)) {
        return;
    }
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    const std::size_t xKey = findKey(object, "x", 0);
    const std::size_t yKey = findKey(object, "y", 0);
    const std::size_t wKey = findKey(object, "w", 0);
    const std::size_t hKey = findKey(object, "h", 0);
    if (xKey != std::string::npos) {
        static_cast<void>(parseIntAfter(object, xKey, x));
    }
    if (yKey != std::string::npos) {
        static_cast<void>(parseIntAfter(object, yKey, y));
    }
    if (wKey != std::string::npos) {
        static_cast<void>(parseIntAfter(object, wKey, width));
    }
    if (hKey != std::string::npos) {
        static_cast<void>(parseIntAfter(object, hKey, height));
    }
    frame.x = x;
    frame.y = y;
    frame.width = width;
    frame.height = height;
}

} // namespace

bool DirectionalAtlas::loadFromFile(const std::string& path) {
    const std::string json = readFile(path);
    if (json.empty()) {
        return false;
    }
    return loadFromString(json);
}

bool DirectionalAtlas::loadFromString(const std::string& json) {
    clips.clear();
    int parsedWidth = 64;
    int parsedHeight = 64;
    int parsedColumns = 1;
    const std::size_t widthKey = findKey(json, "frameWidth", 0);
    const std::size_t heightKey = findKey(json, "frameHeight", 0);
    const std::size_t columnKey = findKey(json, "columns", 0);
    if (widthKey != std::string::npos) {
        static_cast<void>(parseIntAfter(json, widthKey, parsedWidth));
    }
    if (heightKey != std::string::npos) {
        static_cast<void>(parseIntAfter(json, heightKey, parsedHeight));
    }
    if (columnKey != std::string::npos) {
        static_cast<void>(parseIntAfter(json, columnKey, parsedColumns));
    }
    frameWidth = parsedWidth;
    frameHeight = parsedHeight;
    columns = parsedColumns;
    parseObjects<DirectionalClipInfo>(json, "clips", clips, fillDirectional);
    return !clips.empty() && this->frameWidth > 0 && this->frameHeight > 0;
}

const DirectionalClipInfo* DirectionalAtlas::find(const std::string& clipName) const noexcept {
    for (const DirectionalClipInfo& clip : clips) {
        if (clip.name == clipName) {
            return &clip;
        }
    }
    return nullptr;
}

bool NamedAtlas::loadFromFile(const std::string& path) {
    const std::string json = readFile(path);
    if (json.empty()) {
        return false;
    }
    return loadFromString(json);
}

bool NamedAtlas::loadFromString(const std::string& json) {
    frames.clear();
    int width = 0;
    int height = 0;
    const std::size_t widthKey = findKey(json, "imageWidth", 0);
    const std::size_t heightKey = findKey(json, "imageHeight", 0);
    if (widthKey != std::string::npos) {
        static_cast<void>(parseIntAfter(json, widthKey, width));
    }
    if (heightKey != std::string::npos) {
        static_cast<void>(parseIntAfter(json, heightKey, height));
    }
    imageWidth = width;
    imageHeight = height;
    parseObjects<NamedAtlasFrame>(json, "frames", frames, fillNamed);
    return !frames.empty();
}

const NamedAtlasFrame* NamedAtlas::find(const std::string& frameName) const noexcept {
    for (const NamedAtlasFrame& frame : frames) {
        if (frame.name == frameName) {
            return &frame;
        }
    }
    return nullptr;
}

} // namespace render
