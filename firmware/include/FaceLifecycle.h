#pragma once

#include <stdint.h>

// Pure, nonblocking startup/sleep/wake timeline. No Arduino dependencies,
// dynamically allocated buffers or hardware timers: easy to host-test.
// Changes in direction (sleep -> wake -> sleep) begin at the currently
// visible eye opening, so the face never pops to fully open or shut.
namespace deskpet_lifecycle {

enum class Phase : uint8_t {
    Booting,
    Awake,
    FallingAsleep,
    Asleep,
    Waking
};

constexpr uint32_t kBootDurationMs = 1600u;
constexpr uint32_t kBootHoldMs = 230u;
constexpr uint32_t kBootOpenMs = 960u;
constexpr uint32_t kSleepDurationMs = 850u;
constexpr uint32_t kWakeDurationMs = 1050u;
constexpr float kClosedOpenness = 0.055f;

inline const char* phaseName(Phase phase) {
    switch (phase) {
        case Phase::Booting: return "BOOTING";
        case Phase::Awake: return "AWAKE";
        case Phase::FallingAsleep: return "FALLING_ASLEEP";
        case Phase::Asleep: return "ASLEEP";
        case Phase::Waking: return "WAKING";
    }
    return "UNKNOWN";
}

inline float clamp01(float x) {
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

inline float ease(float x) {
    const float t = clamp01(x);
    return t * t * (3.0f - 2.0f * t);
}

class Timeline {
public:
    void begin(uint32_t nowMs) {
        phase_ = Phase::Booting;
        startedMs_ = nowMs;
        startOpening_ = kClosedOpenness;
    }

    Phase phase() const { return phase_; }
    bool sleeping() const {
        return phase_ == Phase::FallingAsleep || phase_ == Phase::Asleep;
    }

    uint32_t elapsed(uint32_t nowMs) const { return nowMs - startedMs_; }

    void advance(uint32_t nowMs) {
        const uint32_t ms = elapsed(nowMs);
        if (phase_ == Phase::Booting && ms >= kBootDurationMs) {
            phase_ = Phase::Awake;
        } else if (phase_ == Phase::FallingAsleep && ms >= kSleepDurationMs) {
            phase_ = Phase::Asleep;
        } else if (phase_ == Phase::Waking && ms >= kWakeDurationMs) {
            phase_ = Phase::Awake;
        }
    }

    float openness(uint32_t nowMs) const {
        const float ms = static_cast<float>(elapsed(nowMs));
        switch (phase_) {
            case Phase::Booting:
                if (ms < kBootHoldMs) return kClosedOpenness;
                return kClosedOpenness + (1.0f - kClosedOpenness) *
                    ease((ms - kBootHoldMs) / kBootOpenMs);
            case Phase::Awake: return 1.0f;
            case Phase::Asleep: return kClosedOpenness;
            case Phase::FallingAsleep:
                return startOpening_ + (kClosedOpenness - startOpening_) *
                    ease(ms / static_cast<float>(kSleepDurationMs));
            case Phase::Waking:
                return startOpening_ + (1.0f - startOpening_) *
                    ease(ms / static_cast<float>(kWakeDurationMs));
        }
        return 1.0f;
    }

    void sleep(uint32_t nowMs) {
        if (sleeping()) return;
        startOpening_ = openness(nowMs);
        phase_ = Phase::FallingAsleep;
        startedMs_ = nowMs;
    }

    void wake(uint32_t nowMs) {
        if (phase_ == Phase::Awake || phase_ == Phase::Waking) return;
        startOpening_ = openness(nowMs);
        phase_ = Phase::Waking;
        startedMs_ = nowMs;
    }

    // A deliberate emotion selection during the intro takes precedence.
    // Continue to open from the present position rather than restarting.
    void interruptBoot(uint32_t nowMs) {
        if (phase_ == Phase::Booting) wake(nowMs);
    }

private:
    Phase phase_ = Phase::Awake;
    uint32_t startedMs_ = 0;
    float startOpening_ = 1.0f;
};

} // namespace deskpet_lifecycle
