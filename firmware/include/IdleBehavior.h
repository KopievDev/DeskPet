#pragma once

#include <stdint.h>

#include "Personality.h"

// Small, deterministic, allocation-free ambient personality scheduler.
// The renderer owns the actual animation; this class only emits changes.
// A reaction, an explicit emotion, demo mode, or sleep can suppress it.
enum class IdleGesture : uint8_t {
    None = 0,
    LookLeft,
    LookRight,
    Squint,
    Yawn,
    Startled,
    BrowBounce,
    Wink
};

class IdleBehavior {
public:
    void begin(uint32_t nowMs, uint32_t seed = 0xD35C9A71u) {
        rng_ = seed ? seed : 0xD35C9A71u;
        current_ = IdleGesture::None;
        active_ = false;
        wasAllowed_ = false;
        startedMs_ = nowMs;
        nextDueMs_ = nowMs;
    }

    bool enabled() const { return enabled_; }
    Personality personality() const { return personality_; }
    IdleGesture gesture() const { return current_; }
    uint32_t startedMs() const { return startedMs_; }
    bool isActive() const { return active_; }

    void setEnabled(bool enabled, uint32_t nowMs) {
        enabled_ = enabled;
        reset(nowMs);
    }

    // Profile changes cancel the current gesture and restart the quiet
    // interval. This does not touch the manually chosen emotion.
    void setPersonality(Personality personality, uint32_t nowMs) {
        personality_ = personality;
        reset(nowMs);
    }

    static uint32_t minInterval(Personality personality) {
        switch (personality) {
            case Personality::Calm: return 14000u;
            case Personality::Playful: return 6000u;
            case Personality::Cheeky: return 9000u;
        }
        return 14000u;
    }

    static uint32_t maxInterval(Personality personality) {
        switch (personality) {
            case Personality::Calm: return 26000u;
            case Personality::Playful: return 12000u;
            case Personality::Cheeky: return 16000u;
        }
        return 26000u;
    }

    // Cancels any ambient action on explicit user interaction. The next
    // gesture will be scheduled after a full quiet interval.
    void reset(uint32_t nowMs) {
        active_ = false;
        current_ = IdleGesture::None;
        wasAllowed_ = false;
        nextDueMs_ = nowMs;
    }

    // Returns true only when the visible gesture actually changed.
    bool tick(uint32_t nowMs, bool eligible) {
        const bool allowed = eligible && enabled_;
        if (!allowed) {
            const bool changed = current_ != IdleGesture::None;
            current_ = IdleGesture::None;
            active_ = false;
            wasAllowed_ = false;
            return changed;
        }

        if (!wasAllowed_) {
            wasAllowed_ = true;
            nextDueMs_ = nowMs + nextInterval();
            return false;
        }

        if (active_) {
            if (static_cast<uint32_t>(nowMs - startedMs_) >= duration(current_)) {
                current_ = IdleGesture::None;
                active_ = false;
                nextDueMs_ = nowMs + nextInterval();
                return true;
            }
            return false;
        }

        if (static_cast<int32_t>(nowMs - nextDueMs_) < 0) return false;
        current_ = nextGesture();
        startedMs_ = nowMs;
        active_ = true;
        return true;
    }

    static uint32_t duration(IdleGesture gesture) {
        switch (gesture) {
            case IdleGesture::LookLeft:
            case IdleGesture::LookRight: return 1900;
            case IdleGesture::Squint: return 1500;
            case IdleGesture::Yawn: return 2450;
            case IdleGesture::Startled: return 1150;
            case IdleGesture::BrowBounce: return 1200;
            case IdleGesture::Wink: return 1350;
            case IdleGesture::None: break;
        }
        return 0;
    }

private:
    uint32_t random32() {
        // xorshift32: repeatable in unit tests and inexpensive on ESP32.
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return rng_;
    }

    uint32_t nextInterval() {
        const uint32_t minMs = minInterval(personality_);
        return minMs + random32() % (maxInterval(personality_) - minMs + 1u);
    }

    IdleGesture nextGesture() {
        // Hand-tuned probabilities (percent); each profile sums to 100.
        // Calm looks around and yawns, Playful winks and raises brows,
        // Cheeky tends to squint, wink and look startled.
        static const uint8_t kCalm[7] = {25, 25, 18, 20, 2, 7, 3};
        static const uint8_t kPlayful[7] = {12, 12, 8, 5, 18, 22, 23};
        static const uint8_t kCheeky[7] = {10, 10, 23, 4, 16, 17, 20};
        const uint8_t* weights = kCalm;
        if (personality_ == Personality::Playful) weights = kPlayful;
        else if (personality_ == Personality::Cheeky) weights = kCheeky;

        uint32_t roll = random32() % 100u;
        for (uint8_t index = 0; index < 7; ++index) {
            if (roll < weights[index]) {
                return static_cast<IdleGesture>(index + 1u);
            }
            roll -= weights[index];
        }
        return IdleGesture::LookLeft; // unreachable if weights sum to 100
    }

    uint32_t rng_ = 0xD35C9A71u;
    uint32_t startedMs_ = 0;
    uint32_t nextDueMs_ = 0;
    IdleGesture current_ = IdleGesture::None;
    bool active_ = false;
    bool enabled_ = true;
    Personality personality_ = Personality::Calm;
    bool wasAllowed_ = false;
};
