#pragma once

#include <cstdint>

namespace engine {

struct FrameProbeSample {
    std::uint32_t glErrorChecks{0};
    std::uint32_t uniformQueries{0};
    std::uint32_t programBinds{0};
    std::uint32_t draws{0};
    std::uint32_t uiQuads{0};
    std::uint32_t textDraws{0};
    double updateMs{0.0};
    double worldMs{0.0};
    double uiMs{0.0};
};

/// Counts GL traffic and CPU stage time while a capture window is open.
class FrameProbe {
public:
    static FrameProbe& instance() noexcept {
        static FrameProbe probe;
        return probe;
    }

    void beginCapture() noexcept {
        capturing_ = true;
        frames_ = 0;
        total_ = {};
    }

    void endCapture() noexcept { capturing_ = false; }

    void beginFrame() noexcept { frame_ = {}; }

    void endFrame() noexcept {
        if (!capturing_) {
            return;
        }
        total_.glErrorChecks += frame_.glErrorChecks;
        total_.uniformQueries += frame_.uniformQueries;
        total_.programBinds += frame_.programBinds;
        total_.draws += frame_.draws;
        total_.uiQuads += frame_.uiQuads;
        total_.textDraws += frame_.textDraws;
        total_.updateMs += frame_.updateMs;
        total_.worldMs += frame_.worldMs;
        total_.uiMs += frame_.uiMs;
        ++frames_;
    }

    void addGlErrorCheck() noexcept { ++frame_.glErrorChecks; }
    void addUniformQuery() noexcept { ++frame_.uniformQueries; }
    void addProgramBind() noexcept { ++frame_.programBinds; }
    void addDraw() noexcept { ++frame_.draws; }
    void addUiQuad() noexcept { ++frame_.uiQuads; }
    void addTextDraw() noexcept { ++frame_.textDraws; }
    void addUpdateMs(const double milliseconds) noexcept { frame_.updateMs += milliseconds; }
    void addWorldMs(const double milliseconds) noexcept { frame_.worldMs += milliseconds; }
    void addUiMs(const double milliseconds) noexcept { frame_.uiMs += milliseconds; }

    [[nodiscard]] int frames() const noexcept { return frames_; }

    [[nodiscard]] FrameProbeSample average() const noexcept {
        FrameProbeSample result{};
        if (frames_ <= 0) {
            return result;
        }
        const double scale = 1.0 / static_cast<double>(frames_);
        result.glErrorChecks = total_.glErrorChecks / static_cast<std::uint32_t>(frames_);
        result.uniformQueries = total_.uniformQueries / static_cast<std::uint32_t>(frames_);
        result.programBinds = total_.programBinds / static_cast<std::uint32_t>(frames_);
        result.draws = total_.draws / static_cast<std::uint32_t>(frames_);
        result.uiQuads = total_.uiQuads / static_cast<std::uint32_t>(frames_);
        result.textDraws = total_.textDraws / static_cast<std::uint32_t>(frames_);
        result.updateMs = total_.updateMs * scale;
        result.worldMs = total_.worldMs * scale;
        result.uiMs = total_.uiMs * scale;
        return result;
    }

private:
    bool capturing_{false};
    int frames_{0};
    FrameProbeSample frame_{};
    FrameProbeSample total_{};
};

} // namespace engine
