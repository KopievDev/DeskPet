#pragma once

#include <stdint.h>

#include "Emotion.h"

// Three tiny scripted face-to-face sketches. The caller owns visible
// priority through ReactionController; this state machine only emits beats.
// No Arduino, NVS, heap, animation buffers, delays or wall-clock time.
namespace deskpet_stories {

enum class Kind : uint8_t { Suspicious, Bedhead, Meltdown, Count };

struct Beat {
    Emotion emotion;
    uint32_t durationMs;
};

inline const char* storyName(Kind kind) {
    switch (kind) {
        case Kind::Suspicious: return "suspicious";
        case Kind::Bedhead: return "bedhead";
        case Kind::Meltdown: return "meltdown";
        case Kind::Count: return "none";
    }
    return "none";
}

inline uint8_t beatCount(Kind kind) {
    switch (kind) {
        case Kind::Suspicious: return 3u;
        case Kind::Bedhead: return 3u;
        case Kind::Meltdown: return 4u;
        case Kind::Count: return 0u;
    }
    return 0u;
}

inline Beat beatAt(Kind kind, uint8_t index) {
    static const Beat kSuspicious[] = {
        {Emotion::SideEye, 1400u},
        {Emotion::Unibrow, 1600u},
        {Emotion::Goofy, 1600u}
    };
    static const Beat kBedhead[] = {
        {Emotion::Scruffy, 1600u},
        {Emotion::Bewildered, 1400u},
        {Emotion::Derp, 1700u}
    };
    static const Beat kMeltdown[] = {
        {Emotion::Surprised, 1000u},
        {Emotion::Panic, 1200u},
        {Emotion::Puffed, 1200u},
        {Emotion::Happy, 1400u}
    };
    if (index >= beatCount(kind)) return {Emotion::Neutral, 0u};
    switch (kind) {
        case Kind::Suspicious: return kSuspicious[index];
        case Kind::Bedhead: return kBedhead[index];
        case Kind::Meltdown: return kMeltdown[index];
        case Kind::Count: break;
    }
    return {Emotion::Neutral, 0u};
}

class Director {
public:
    void begin() {
        active_ = false;
        autoEnabled_ = true;
        next_ = Kind::Suspicious;
        current_ = Kind::Count;
        stage_ = 0u;
        startedMs_ = 0u;
        played_ = 0u;
    }

    bool active() const { return active_; }
    bool autoEnabled() const { return autoEnabled_; }
    uint32_t played() const { return played_; }
    Kind current() const { return current_; }
    uint8_t stage() const { return stage_; }
    Beat currentBeat() const {
        return active_ ? beatAt(current_, stage_) : Beat{Emotion::Neutral, 0u};
    }

    void setAutoEnabled(bool enabled) { autoEnabled_ = enabled; }

    void cancel() {
        active_ = false;
        current_ = Kind::Count;
        stage_ = 0u;
    }

    // Called only when neutral, awake and idle; the caller additionally
    // arbitrates against host/build/needs/bond reactions.
    bool startNext(uint32_t nowMs, Beat& first) {
        if (active_) return false;
        const Kind chosen = next_;
        next_ = static_cast<Kind>((static_cast<uint8_t>(next_) + 1u) %
                                  static_cast<uint8_t>(Kind::Count));
        current_ = chosen;
        stage_ = 0u;
        startedMs_ = nowMs;
        first = beatAt(current_, stage_);
        active_ = true;
        if (played_ != UINT32_MAX) ++played_;
        return true;
    }

    // Advance at most one beat. Every beat uses a fresh temporary reaction
    // duration, so an interrupt can immediately restore the manual face.
    // When the final beat expires, ReactionController returns to Neutral.
    bool tick(uint32_t nowMs, bool permitted, Beat& nextBeat) {
        if (!active_) return false;
        if (!permitted) {
            cancel();
            return false;
        }
        const uint32_t elapsed = nowMs - startedMs_;
        if (elapsed < currentBeat().durationMs) return false;
        // Do not replay overdue scenes after a paused loop or a reset.
        if (elapsed > 10000u || stage_ + 1u >= beatCount(current_)) {
            cancel();
            return false;
        }
        ++stage_;
        startedMs_ = nowMs;
        nextBeat = currentBeat();
        return true;
    }

private:
    Kind current_ = Kind::Count;
    Kind next_ = Kind::Suspicious;
    uint32_t startedMs_ = 0u;
    uint32_t played_ = 0u;
    uint8_t stage_ = 0u;
    bool active_ = false;
    bool autoEnabled_ = true;
};

} // namespace deskpet_stories
