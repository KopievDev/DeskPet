#pragma once

#include <stdint.h>

#include "Emotion.h"
#include "Personality.h"

// Explicit host activity hints, never inferred from ESP32 button presses.
// Host sends only transitions ("event user-idle" / "event user-active").
// Completely inert until the first host event; no keyboard or personal data.
namespace deskpet_activity {

enum class Presence : uint8_t { Unknown, Active, Away };

struct Cue {
    Emotion emotion;
    uint32_t durationMs;
};

class Controller {
public:
    static constexpr uint32_t kIdleJokeIntervalMs = 90000u;
    static constexpr uint32_t kPendingLifetimeMs = 12000u;
    static constexpr uint32_t kComebackMinimumMs = 10000u;
    static constexpr uint32_t kComicDurationMs = 4800u;
    static constexpr uint32_t kShortDurationMs = 3000u;

    void begin(uint32_t nowMs) {
        enabled_ = true;
        state_ = Presence::Unknown;
        pending_ = Pending::None;
        pendingAtMs_ = nowMs;
        awaySinceMs_ = nowMs;
        nextJokeMs_ = nowMs;
        jokes_ = 0;
    }

    bool enabled() const { return enabled_; }
    Presence presence() const { return state_; }
    uint8_t jokeCount() const { return jokes_; }

    void setEnabled(bool enabled, uint32_t nowMs) {
        enabled_ = enabled;
        pending_ = Pending::None;
        state_ = Presence::Unknown;
        nextJokeMs_ = nowMs;
        jokes_ = 0;
    }

    // No action on the very first event: avoid a spontaneous reaction
    // when the Mac helper first connects.
    void idle(uint32_t nowMs) {
        if (!enabled_ || state_ == Presence::Away) return;
        const bool firstHostEvent = state_ == Presence::Unknown;
        state_ = Presence::Away;
        awaySinceMs_ = nowMs;
        nextJokeMs_ = nowMs + kIdleJokeIntervalMs;
        jokes_ = 0;
        pending_ = firstHostEvent ? Pending::None : Pending::Idle;
        pendingAtMs_ = nowMs;
    }

    void active(uint32_t nowMs) {
        if (!enabled_) return;
        if (state_ == Presence::Away &&
            static_cast<uint32_t>(nowMs - awaySinceMs_) >=
                kComebackMinimumMs) {
            pending_ = Pending::Return;
            pendingAtMs_ = nowMs;
        } else {
            pending_ = Pending::None;
        }
        state_ = Presence::Active;
        jokes_ = 0;
    }

    // Caller passes eligibility only when: awake, no Demo, manual Neutral,
    // no active build/USER reaction and no pending idle reaction.
    // Drop stale pending cues rather than surprise the owner after a build.
    bool tick(uint32_t nowMs, bool eligible, Personality personality,
              uint8_t affection, Cue& result) {
        if (!enabled_) return false;

        if (pending_ != Pending::None) {
            if (nowMs - pendingAtMs_ > kPendingLifetimeMs) {
                pending_ = Pending::None;
            } else if (eligible) {
                const Pending pending = pending_;
                pending_ = Pending::None;
                if (pending == Pending::Idle) {
                    result = {Emotion::Bored, kShortDurationMs};
                } else if (affection >= 75u) {
                    result = {Emotion::Love, kShortDurationMs};
                } else if (personality == Personality::Cheeky) {
                    result = {Emotion::Derp, kComicDurationMs};
                } else {
                    result = {Emotion::Excited, kShortDurationMs};
                }
                return true;
            }
        }

        if (state_ != Presence::Away || !eligible ||
            static_cast<int32_t>(nowMs - nextJokeMs_) < 0) return false;

        nextJokeMs_ = nowMs + kIdleJokeIntervalMs;
        if (jokes_ < 255u) ++jokes_;
        // Gentle escalating jokes, never Furious/Angry for neglect.
        if (personality == Personality::Calm) {
            result = {jokes_ % 2u ? Emotion::Nerd : Emotion::Bored,
                      kComicDurationMs};
        } else if (personality == Personality::Cheeky) {
            result = {jokes_ % 2u ? Emotion::Gremlin : Emotion::Derp,
                      kComicDurationMs};
        } else {
            result = {jokes_ % 2u ? Emotion::Derp : Emotion::Gremlin,
                      kComicDurationMs};
        }
        return true;
    }

private:
    enum class Pending : uint8_t { None, Idle, Return };

    Presence state_ = Presence::Unknown;
    Pending pending_ = Pending::None;
    uint32_t pendingAtMs_ = 0u;
    uint32_t awaySinceMs_ = 0u;
    uint32_t nextJokeMs_ = 0u;
    uint8_t jokes_ = 0u;
    bool enabled_ = true;
};

inline const char* presenceName(Presence state) {
    switch (state) {
        case Presence::Unknown: return "UNKNOWN";
        case Presence::Active: return "ACTIVE";
        case Presence::Away: return "AWAY";
    }
    return "UNKNOWN";
}

} // namespace deskpet_activity
