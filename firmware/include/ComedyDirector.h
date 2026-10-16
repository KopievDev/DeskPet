#pragma once

#include <stdint.h>

#include "Emotion.h"
#include "Personality.h"

// Tiny, allocation-free director for optional idle comedy.
// Eligible time accrues only when no higher-priority action is playing.
// This avoids starvation from bond/needs scenes, and never queues skits.
namespace deskpet_comedy {

struct Cue {
    Emotion emotion;
    uint32_t durationMs;
};

class Director {
public:
    static constexpr uint32_t kSceneDurationMs = 4800u;

    void begin(uint32_t nowMs, uint32_t seed = 0xC0B1C0DEu) {
        rng_ = seed == 0u ? 0xC0B1C0DEu : seed;
        enabled_ = true;
        count_ = 0u;
        lastEmotion_ = Emotion::Count;
        reset(nowMs);
    }

    bool enabled() const { return enabled_; }
    uint32_t count() const { return count_; }
    Emotion lastEmotion() const { return lastEmotion_; }
    uint32_t eligibleMs() const { return elapsedEligibleMs_; }
    uint32_t targetMs() const { return intervalMs_; }
    uint32_t remainingMs() const {
        return elapsedEligibleMs_ >= intervalMs_
            ? 0u : intervalMs_ - elapsedEligibleMs_;
    }

    static uint32_t minimumInterval(Personality personality) {
        switch (personality) {
            case Personality::Calm: return 100000u;
            case Personality::Playful: return 55000u;
            case Personality::Cheeky: return 70000u;
        }
        return 100000u;
    }

    static uint32_t maximumInterval(Personality personality) {
        switch (personality) {
            case Personality::Calm: return 150000u;
            case Personality::Playful: return 85000u;
            case Personality::Cheeky: return 105000u;
        }
        return 150000u;
    }

    void reset(uint32_t nowMs) {
        previousMs_ = nowMs;
        elapsedEligibleMs_ = 0u;
        wasEligible_ = false;
        intervalMs_ = nextInterval(Personality::Calm);
    }

    void setEnabled(bool enabled, uint32_t nowMs) {
        enabled_ = enabled;
        reset(nowMs);
    }

    // Explicit interaction, manual expression, sleep and demo may call this
    // to begin a fresh quiet-time window. The last act is retained, so the
    // next random scene cannot be an exact repeat.
    void activity(uint32_t nowMs) {
        reset(nowMs);
    }

    // 'eligible' reflects other systems' priority, not the current emotion.
    // Time spent in temporary scenes, Mac-away, or ambient gestures is paused.
    bool tick(uint32_t nowMs, bool eligible, Personality personality,
              uint8_t energy, uint8_t mood, uint8_t attention,
              uint8_t affection, Cue& cue) {
        const uint32_t delta = nowMs - previousMs_;
        previousMs_ = nowMs;
        if (!enabled_ || !eligible) {
            wasEligible_ = false;
            return false;
        }
        if (!wasEligible_) {
            wasEligible_ = true;
            return false;
        }
        // A delayed/blocked scheduler must not invent hours of idle time.
        if (delta > 2000u) return false;
        if (elapsedEligibleMs_ < intervalMs_)
            elapsedEligibleMs_ += delta;
        if (elapsedEligibleMs_ < intervalMs_) return false;
        emit(nowMs, personality, energy, mood, attention, affection, cue);
        return true;
    }

    // Interactive preview only: the caller must enforce sleep, demo, manual
    // selection and temporary-event priority before calling.
    bool preview(uint32_t nowMs, Personality personality,
                 uint8_t energy, uint8_t mood, uint8_t attention,
                 uint8_t affection, Cue& cue) {
        emit(nowMs, personality, energy, mood, attention, affection, cue);
        return true;
    }

private:
    uint32_t random32() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return rng_;
    }

    uint32_t nextInterval(Personality personality) {
        const uint32_t lo = minimumInterval(personality);
        const uint32_t hi = maximumInterval(personality);
        return lo + random32() % (hi - lo + 1u);
    }

    Emotion select(Personality personality, uint8_t energy, uint8_t mood,
                   uint8_t attention, uint8_t affection) {
        // Weighted personality palette: five non-repeating cartoon acts.
        uint8_t weights[5] = {22u, 18u, 24u, 18u, 18u};
        if (personality == Personality::Playful) {
            weights[0] = 17u; weights[1] = 24u; weights[2] = 13u;
            weights[3] = 18u; weights[4] = 28u;
        } else if (personality == Personality::Cheeky) {
            weights[0] = 32u; weights[1] = 14u; weights[2] = 29u;
            weights[3] = 14u; weights[4] = 11u;
        }
        if (energy <= 25u) weights[1] += 15u; // scruffy when tired
        if (mood <= 30u) weights[2] += 15u;   // skeptical stare
        if (attention <= 25u) weights[3] += 15u; // puffed cheeks
        if (affection >= 75u) weights[4] += 12u; // goofy when loved

        static const Emotion kChoices[] = {
            Emotion::Unibrow, Emotion::Scruffy, Emotion::SideEye,
            Emotion::Puffed, Emotion::Goofy
        };
        uint16_t total = 0u;
        for (uint8_t i = 0u; i < 5u; ++i) {
            if (kChoices[i] == lastEmotion_) weights[i] = 0u;
            total += weights[i];
        }
        uint32_t roll = random32() % total;
        for (uint8_t i = 0u; i < 5u; ++i) {
            if (roll < weights[i]) return kChoices[i];
            roll -= weights[i];
        }
        return Emotion::Unibrow; // unreachable for positive total
    }

    void emit(uint32_t nowMs, Personality personality,
              uint8_t energy, uint8_t mood, uint8_t attention,
              uint8_t affection, Cue& cue) {
        const Emotion selection =
            select(personality, energy, mood, attention, affection);
        lastEmotion_ = selection;
        if (count_ != UINT32_MAX) ++count_;
        cue = {selection, kSceneDurationMs};
        elapsedEligibleMs_ = 0u;
        intervalMs_ = nextInterval(personality);
        previousMs_ = nowMs;
        wasEligible_ = false;
    }

    uint32_t rng_ = 0xC0B1C0DEu;
    uint32_t previousMs_ = 0u;
    uint32_t elapsedEligibleMs_ = 0u;
    uint32_t intervalMs_ = 100000u;
    uint32_t count_ = 0u;
    Emotion lastEmotion_ = Emotion::Count;
    bool enabled_ = true;
    bool wasEligible_ = false;
};

} // namespace deskpet_comedy
