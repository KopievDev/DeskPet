#pragma once

#include <stdint.h>

#include "Emotion.h"
#include "Personality.h"

// Lightweight Tamagotchi-style needs: energy, mood, attention.
// Numbers are gameplay state, not a measurement of a person's wellbeing.
// No storage, Wi-Fi, sensors, blocking waits, or background tasks.
namespace deskpet_needs {

enum class Kind : uint8_t { None, Coffee, Attention, Cheer };
struct Cue { Emotion emotion; Kind kind; uint32_t durationMs; };

class Controller {
public:
    static constexpr uint32_t kEnergyAwakeMs = 240000u; // -1 every 4 min
    static constexpr uint32_t kEnergySleepMs = 60000u; // +1 every 1 min
    static constexpr uint32_t kAttentionMs = 240000u; // -1 every 4 min awake
    static constexpr uint32_t kMoodMs = 540000u; // -1 every 9 min awake
    static constexpr uint32_t kCueIntervalMs = 90000u;
    static constexpr uint32_t kCueDurationMs = 4800u;

    void begin(uint32_t nowMs) {
        enabled_ = true;
        reset(nowMs);
    }

    void reset(uint32_t nowMs) {
        energy_ = 78u;
        mood_ = 70u;
        attention_ = 65u;
        lastUpdateMs_ = nowMs;
        energyCarryMs_ = 0u;
        attentionCarryMs_ = 0u;
        moodCarryMs_ = 0u;
        nextCueMs_ = nowMs + kCueIntervalMs;
        wasEligible_ = false;
        lastSleeping_ = false;
    }

    uint8_t energy() const { return energy_; }
    uint8_t mood() const { return mood_; }
    uint8_t attention() const { return attention_; }
    bool enabled() const { return enabled_; }

    void setEnabled(bool enabled, uint32_t nowMs) {
        enabled_ = enabled;
        lastUpdateMs_ = nowMs;
        energyCarryMs_ = 0u;
        attentionCarryMs_ = 0u;
        moodCarryMs_ = 0u;
        wasEligible_ = false;
        lastSleeping_ = false;
        nextCueMs_ = nowMs + kCueIntervalMs;
    }

    // Manual test mode: values are always bounded 0..100.
    void set(uint8_t energy, uint8_t mood, uint8_t attention,
             uint32_t nowMs) {
        energy_ = energy > 100u ? 100u : energy;
        mood_ = mood > 100u ? 100u : mood;
        attention_ = attention > 100u ? 100u : attention;
        lastUpdateMs_ = nowMs;
        energyCarryMs_ = 0u;
        attentionCarryMs_ = 0u;
        moodCarryMs_ = 0u;
        wasEligible_ = false;
        nextCueMs_ = nowMs + kCueIntervalMs;
    }

    void pet() {
        if (!enabled_) return;
        mood_ = add(mood_, 12u);
        attention_ = add(attention_, 20u);
    }

    void interact() {
        if (!enabled_) return;
        attention_ = add(attention_, 3u);
    }

    void surprise() {
        if (!enabled_) return;
        mood_ = add(mood_, 4u);
        attention_ = add(attention_, 8u);
    }

    void coffee() {
        if (!enabled_) return;
        energy_ = add(energy_, 25u);
        mood_ = add(mood_, 5u);
    }

    void play() {
        if (!enabled_) return;
        energy_ = subtract(energy_, 3u);
        mood_ = add(mood_, 10u);
        attention_ = add(attention_, 18u);
    }

    void build(bool success) {
        if (!enabled_) return;
        if (success) mood_ = add(mood_, 5u);
        else {
            mood_ = subtract(mood_, 6u);
            energy_ = subtract(energy_, 1u);
        }
    }

    // Update continuously from millis(). Sleeping restores energy and
    // pauses attention/mood decay. No time is "made up" after waking.
    void advance(uint32_t nowMs, bool sleeping) {
        const uint32_t delta = nowMs - lastUpdateMs_;
        lastUpdateMs_ = nowMs;
        if (sleeping != lastSleeping_) {
            energyCarryMs_ = 0u;
            attentionCarryMs_ = 0u;
            moodCarryMs_ = 0u;
            lastSleeping_ = sleeping;
        }
        if (!enabled_) return;

        // Reject intervals larger than 12 hours: likely a reset,
        // missed timer, or an uninitialized caller. Rollover with
        // normal frame delta is naturally safe via uint32 subtraction.
        if (delta > 43200000u) {
            energyCarryMs_ = 0u;
            attentionCarryMs_ = 0u;
            moodCarryMs_ = 0u;
            return;
        }
        const uint32_t energyInterval =
            sleeping ? kEnergySleepMs : kEnergyAwakeMs;
        energyCarryMs_ += delta;
        const uint32_t energyPoints = energyCarryMs_ / energyInterval;
        energyCarryMs_ %= energyInterval;
        if (sleeping) {
            energy_ = add(energy_, energyPoints);
            attentionCarryMs_ = 0u;
            moodCarryMs_ = 0u;
            wasEligible_ = false;
            return;
        }
        energy_ = subtract(energy_, energyPoints);
        attentionCarryMs_ += delta;
        moodCarryMs_ += delta;
        const uint32_t attentionPoints = attentionCarryMs_ / kAttentionMs;
        const uint32_t moodPoints = moodCarryMs_ / kMoodMs;
        attentionCarryMs_ %= kAttentionMs;
        moodCarryMs_ %= kMoodMs;
        attention_ = subtract(attention_, attentionPoints);
        mood_ = subtract(mood_, moodPoints);
    }

    // Eligibility is controlled by the caller: only if awake, Neutral,
    // no demo, temporary/build/USER reaction or Mac activity skit.
    // Never forces Sleep; users retain full control of the BOOT button.
    bool tick(uint32_t nowMs, bool eligible, Personality personality,
              Cue& cue) {
        if (!enabled_ || !eligible) {
            wasEligible_ = false;
            return false;
        }
        if (!wasEligible_) {
            wasEligible_ = true;
            nextCueMs_ = nowMs + kCueIntervalMs;
            return false;
        }
        if (static_cast<int32_t>(nowMs - nextCueMs_) < 0) return false;
        nextCueMs_ = nowMs + kCueIntervalMs;

        if (energy_ <= 25u) {
            cue = {Emotion::Nerd, Kind::Coffee, kCueDurationMs};
            return true;
        }
        if (attention_ <= 25u) {
            cue = {personality == Personality::Cheeky
                       ? Emotion::Gremlin : Emotion::Derp,
                   Kind::Attention, kCueDurationMs};
            return true;
        }
        if (mood_ <= 25u) {
            cue = {Emotion::Bewildered, Kind::Cheer, kCueDurationMs};
            return true;
        }
        return false;
    }

private:
    static uint8_t add(uint8_t value, uint32_t points) {
        const uint32_t result = static_cast<uint32_t>(value) + points;
        return static_cast<uint8_t>(result > 100u ? 100u : result);
    }

    static uint8_t subtract(uint8_t value, uint32_t points) {
        return points >= value ? 0u :
            static_cast<uint8_t>(value - points);
    }

    uint32_t lastUpdateMs_ = 0u;
    uint32_t energyCarryMs_ = 0u;
    uint32_t attentionCarryMs_ = 0u;
    uint32_t moodCarryMs_ = 0u;
    uint32_t nextCueMs_ = 0u;
    uint8_t energy_ = 78u;
    uint8_t mood_ = 70u;
    uint8_t attention_ = 65u;
    bool enabled_ = true;
    bool wasEligible_ = false;
    bool lastSleeping_ = false;
};

} // namespace deskpet_needs
