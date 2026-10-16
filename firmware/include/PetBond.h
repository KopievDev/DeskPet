#pragma once

#include <stdint.h>

#include "Emotion.h"
#include "Personality.h"

// Tiny non-persistent Tamagotchi-style affection score.
// All changes are explicit user/build events or slow awake-time decay.
// Occasional "comic cameos" are scheduled only when the caller confirms
// idle Neutral and no competing action. No heap, Arduino, Wi-Fi or NVS.
class PetBond {
public:
    static constexpr uint8_t kInitialScore = 50u;
    static constexpr uint32_t kDecayMs = 600000u; // 1 point / 10 min awake
    static constexpr uint32_t kInteractionCooldownMs = 1800u;
    static constexpr uint32_t kComicDurationMs = 2700u;

    void begin(uint32_t nowMs, uint32_t seed = 0x196C23AFu) {
        score_ = kInitialScore;
        enabled_ = true;
        lastDecayMs_ = nowMs;
        lastInteractionMs_ = nowMs - kInteractionCooldownMs;
        rng_ = seed ? seed : 0x196C23AFu;
        armed_ = false;
        nextComicMs_ = nowMs;
    }

    // Restore saved score, not scheduler timestamps or RNG state.
    void restore(uint8_t score, uint32_t nowMs) {
        score_ = score > 100u ? 100u : score;
        lastDecayMs_ = nowMs;
        lastInteractionMs_ = nowMs - kInteractionCooldownMs;
        nextComicMs_ = nowMs;
        armed_ = false;
    }

    uint8_t score() const { return score_; }
    bool enabled() const { return enabled_; }

    void setEnabled(bool enabled, uint32_t nowMs) {
        enabled_ = enabled;
        armed_ = false;
        nextComicMs_ = nowMs;
    }

    void reset(uint32_t nowMs) {
        score_ = kInitialScore;
        lastDecayMs_ = nowMs;
        lastInteractionMs_ = nowMs - kInteractionCooldownMs;
        armed_ = false;
    }

    void interact(uint32_t nowMs) {
        // Ordinary taps shouldn't create runaway affection from button spam.
        if (static_cast<uint32_t>(nowMs - lastInteractionMs_) >=
            kInteractionCooldownMs) add(1);
        lastInteractionMs_ = nowMs;
        armed_ = false;
    }

    void surprise(uint32_t nowMs) {
        add(2);
        lastInteractionMs_ = nowMs;
        armed_ = false;
    }

    void pet(uint32_t nowMs) {
        add(9);
        lastInteractionMs_ = nowMs;
        armed_ = false;
    }

    void build(bool success, uint32_t nowMs) {
        if (success) add(2);
        else subtract(1);
        armed_ = false;
        (void)nowMs;
    }

    // Call continuously. "awake" pauses decay while the pet is sleeping.
    // "eligible" requires awake, Neutral, no demo/temporary reaction/Bored.
    // A true return emits one cameo; show it through ReactionController so
    // the manually selected expression returns automatically afterward.
    bool tick(uint32_t nowMs, bool awake, bool eligible,
              Personality personality, Emotion& cameo) {
        if (!awake) {
            lastDecayMs_ = nowMs; // pause decay, no catch-up after sleep
            armed_ = false;
            return false;
        }

        const uint32_t elapsed = nowMs - lastDecayMs_;
        if (elapsed >= kDecayMs) {
            const uint32_t points = elapsed / kDecayMs;
            subtract(points > 100u ? 100u : static_cast<uint8_t>(points));
            lastDecayMs_ += points * kDecayMs;
        }

        if (!enabled_ || !eligible) {
            armed_ = false;
            return false;
        }
        if (!armed_) {
            armed_ = true;
            nextComicMs_ = nowMs + nextInterval();
            return false;
        }
        if (static_cast<int32_t>(nowMs - nextComicMs_) < 0) return false;
        armed_ = false;
        cameo = pickComic(personality);
        return true;
    }

private:
    void add(uint8_t points) {
        const uint16_t sum = static_cast<uint16_t>(score_) + points;
        score_ = sum > 100u ? 100u : static_cast<uint8_t>(sum);
    }
    void subtract(uint8_t points) {
        score_ = score_ > points ? static_cast<uint8_t>(score_ - points) : 0u;
    }

    uint32_t random32() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return rng_;
    }

    uint32_t nextInterval() {
        // Infrequent: one optional 2.7s cameo per 55-85 seconds of
        // uninterrupted calm. IdleBehavior continues independently.
        return 55000u + random32() % 30001u;
    }

    Emotion pickComic(Personality personality) {
        const bool flip = (random32() & 1u) != 0u;
        if (score_ <= 25u) {
            return personality == Personality::Calm ? Emotion::Panic
                                                    : Emotion::Gremlin;
        }
        if (score_ >= 75u) {
            return personality == Personality::Cheeky
                ? (flip ? Emotion::Gremlin : Emotion::Derp)
                : (flip ? Emotion::Derp : Emotion::Nerd);
        }
        switch (personality) {
            case Personality::Calm: return Emotion::Nerd;
            case Personality::Playful: return flip ? Emotion::Derp : Emotion::Nerd;
            case Personality::Cheeky: return flip ? Emotion::Gremlin : Emotion::Derp;
        }
        return Emotion::Nerd;
    }

    uint32_t rng_ = 0x196C23AFu;
    uint32_t lastDecayMs_ = 0;
    uint32_t lastInteractionMs_ = 0;
    uint32_t nextComicMs_ = 0;
    uint8_t score_ = kInitialScore;
    bool enabled_ = true;
    bool armed_ = false;
};
