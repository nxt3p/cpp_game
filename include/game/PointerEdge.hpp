#pragma once

namespace game {

struct PointerEdgeState {
    bool pressed{false};
    bool released{false};
    bool mouseWasDown{false};
};

/// One logical click from a synthetic press plus a still-held physical button.
/// A synthetic press with the button already up is a full click (press and release)
/// so scripted drags still complete in the same frame. A held button does not press again.
[[nodiscard]] inline PointerEdgeState advancePointerEdge(
    const bool syntheticClick,
    const bool physicalDown,
    const bool mouseWasDown) noexcept {
    PointerEdgeState edge;
    edge.pressed = syntheticClick || (physicalDown && !mouseWasDown);
    edge.released = (syntheticClick && !physicalDown) || (!physicalDown && mouseWasDown);
    edge.mouseWasDown = physicalDown;
    return edge;
}

} // namespace game
