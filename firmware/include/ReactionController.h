#pragma once

#include <stdint.h>

#include "Emotion.h"

// Pure state machine (no Arduino, allocations, timers, or IO). Reacts to
// explicit build results and optional inactivity while preserving the
// user's last manually chosen emotion.
class ReactionController {
public:
    static constexpr uint32_t kReactionDurationMs = 4500;
    static constexpr uint32_t kDefaultIdleSeconds = 120;
    static constexpr uint32_t kMinIdleSeconds = 15;
    static constexpr uint32_t kMaxIdleSeconds = 3600;

    void begin(uint32_t nowMs) {
        lastActivityMs_ = nowMs;
        desired_ = manual_;
        lastEmitted_ = manual_;
        dirty_ = false;
        reactionActive_ = false;
        idleActive_ = false;
    }

    Emotion manualEmotion() const { return manual_; }
    Emotion desiredEmotion() const { return desired_; }
    bool idleEnabled() const { return idleEnabled_; }
    uint32_t idleSeconds() const { return idleSeconds_; }
    bool reactionActive() const { return reactionActive_; }
    bool idleActive() const { return idleActive_; }

    void chooseEmotion(Emotion emotion, uint32_t nowMs) {
        if (emotion == Emotion::Count) return;
        manual_ = emotion;
        activity(nowMs);
    }

    void nextEmotion(uint32_t nowMs) {
        const auto index = static_cast<uint8_t>(manual_);
        const auto count = static_cast<uint8_t>(Emotion::Count);
        chooseEmotion(static_cast<Emotion>((index + 1u) % count), nowMs);
    }

    // UI input and waking up cancel transient reactions, but do not replace
    // the manually selected baseline with Furious / Excited / Bored.
    void activity(uint32_t nowMs) {
        lastActivityMs_ = nowMs;
        reactionActive_ = false;
        idleActive_ = false;
        setDesired(manual_);
    }

    // Temporary expressions never replace the manually selected mood.
    // Restarting an active reaction extends its lifetime and cannot
    // interrupt Sleep by itself (the caller enforces sleep priority).
    void showTemporary(Emotion emotion, uint32_t nowMs, uint32_t durationMs) {
        if (emotion == Emotion::Count) return;
        lastActivityMs_ = nowMs;
        idleActive_ = false;
        reactionActive_ = true;
        reactionStartedMs_ = nowMs;
        reactionDurationMs_ = durationMs < 250u ? 250u
                            : (durationMs > 10000u ? 10000u : durationMs);
        setDesired(emotion);
    }

    void buildResult(bool success, uint32_t nowMs) {
        showTemporary(success ? Emotion::Excited : Emotion::Furious,
                      nowMs, kReactionDurationMs);
    }

    void setIdleEnabled(bool enabled, uint32_t nowMs) {
        idleEnabled_ = enabled;
        activity(nowMs);
    }

    bool setIdleSeconds(uint32_t seconds, uint32_t nowMs) {
        if (seconds < kMinIdleSeconds || seconds > kMaxIdleSeconds) return false;
        idleSeconds_ = seconds;
        activity(nowMs);
        return true;
    }

    void tick(uint32_t nowMs) {
        if (reactionActive_ &&
            static_cast<uint32_t>(nowMs - reactionStartedMs_) >= reactionDurationMs_) {
            reactionActive_ = false;
            setDesired(manual_);
        }
        if (!reactionActive_ && idleEnabled_ && !idleActive_ &&
            static_cast<uint32_t>(nowMs - lastActivityMs_) >= idleSeconds_ * 1000u) {
            idleActive_ = true;
            setDesired(Emotion::Bored);
        }
    }

    // Emit each change once: calling FaceAnimator::setEmotion continuously
    // would restart its animation and never allow a transition to finish.
    bool takeChange(Emotion& out) {
        if (!dirty_) return false;
        dirty_ = false;
        lastEmitted_ = desired_;
        out = desired_;
        return true;
    }

private:
    void setDesired(Emotion emotion) {
        if (desired_ == emotion) return;
        desired_ = emotion;
        dirty_ = true;
    }

    Emotion manual_ = Emotion::Neutral;
    Emotion desired_ = Emotion::Neutral;
    Emotion lastEmitted_ = Emotion::Neutral;
    uint32_t lastActivityMs_ = 0;
    uint32_t reactionStartedMs_ = 0;
    uint32_t reactionDurationMs_ = kReactionDurationMs;
    uint32_t idleSeconds_ = kDefaultIdleSeconds;
    bool idleEnabled_ = true;
    bool idleActive_ = false;
    bool reactionActive_ = false;
    bool dirty_ = false;
};
