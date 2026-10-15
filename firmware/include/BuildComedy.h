#pragma once

#include <stdint.h>

#include "Emotion.h"

// A tiny comedy arc for build events: first mistake -> panic; second ->
// sneaky gremlin; repeated failures -> furious / gremlin; fixing it ->
// triumphant Nerd. Manual emotions, sleeping and demo retain priority in
// main.cpp / ReactionController. No heap, timers, NVS or network access.
namespace deskpet_build_comedy {

struct Cue {
    Emotion emotion;
    uint32_t durationMs;
};

class Story {
public:
    static constexpr uint32_t kStreakWindowMs = 600000u; // 10 minutes
    static constexpr uint32_t kComicDurationMs = 4800u; // full acting scene
    static constexpr uint32_t kFuriousDurationMs = 4500u;
    static constexpr uint32_t kExcitedDurationMs = 4500u;

    void begin() { reset(); }

    void reset() {
        failures_ = 0u;
        lastFailureMs_ = 0u;
        hadFailure_ = false;
        nextFurious_ = true;
    }

    uint8_t consecutiveFailures() const { return failures_; }

    Cue record(bool success, uint32_t nowMs) {
        if (success) {
            // A success after a stale failure should be a normal celebration,
            // not an apparently unmotivated Nerd brag.
            const bool recovery = isRecent(nowMs);
            reset();
            return recovery ? Cue{Emotion::Nerd, kComicDurationMs}
                            : Cue{Emotion::Excited, kExcitedDurationMs};
        }

        if (!isRecent(nowMs)) {
            failures_ = 0u;
            nextFurious_ = true;
        }
        if (failures_ < 99u) ++failures_;
        lastFailureMs_ = nowMs;
        hadFailure_ = true;

        if (failures_ == 1u) return {Emotion::Panic, kComicDurationMs};
        if (failures_ == 2u) return {Emotion::Gremlin, kComicDurationMs};
        // Alternate even after the visible counter saturates at 99.
        const bool furious = nextFurious_;
        nextFurious_ = !nextFurious_;
        return furious ? Cue{Emotion::Furious, kFuriousDurationMs}
                       : Cue{Emotion::Gremlin, kComicDurationMs};
    }

private:
    bool isRecent(uint32_t nowMs) const {
        return hadFailure_ &&
               static_cast<uint32_t>(nowMs - lastFailureMs_) <=
                   kStreakWindowMs;
    }

    uint32_t lastFailureMs_ = 0u;
    uint8_t failures_ = 0u;
    bool hadFailure_ = false;
    bool nextFurious_ = true;
};

} // namespace deskpet_build_comedy
