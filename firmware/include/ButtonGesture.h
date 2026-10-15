#pragma once

#include <stdint.h>

// Active-low GPIO is converted to a pressed bool by the caller. This pure
// state machine debounces both edges and separates short/double/long actions.
// No Arduino API, interrupts, blocking waits, allocations or hardware timers.
namespace deskpet_button {

enum class Action : uint8_t {
    None,
    Tap,
    DoubleTap,
    Hold
};

class Gesture {
public:
    static constexpr uint32_t kDebounceMs = 35u;
    static constexpr uint32_t kHoldMs = 850u;
    static constexpr uint32_t kDoubleTapMs = 320u;

    // A USER button supports double-tap; BOOT uses immediate single-tap
    // on RELEASE and long hold, with no unnecessary double-click latency.
    explicit Gesture(bool allowDoubleTap) : allowDoubleTap_(allowDoubleTap) {}

    void begin(bool pressed, uint32_t nowMs) {
        raw_ = pressed;
        stable_ = pressed;
        changedMs_ = nowMs;
        pressedMs_ = nowMs;
        releasedMs_ = nowMs;
        pendingTap_ = false;
        longFired_ = pressed; // holding BOOT during reset must not fire
    }

    Action poll(bool pressed, uint32_t nowMs) {
        if (pressed != raw_) {
            raw_ = pressed;
            changedMs_ = nowMs;
        }
        if (raw_ != stable_ && nowMs - changedMs_ >= kDebounceMs) {
            stable_ = raw_;
            if (stable_) {
                pressedMs_ = nowMs;
                longFired_ = false;
            } else if (!longFired_) {
                if (!allowDoubleTap_) return Action::Tap;

                if (pendingTap_ && pressedMs_ - releasedMs_ <= kDoubleTapMs) {
                    pendingTap_ = false;
                    return Action::DoubleTap;
                }
                // If a second press was too late, return the old tap and
                // remember this new tap for the next poll interval.
                if (pendingTap_) {
                    releasedMs_ = nowMs;
                    return Action::Tap;
                }
                pendingTap_ = true;
                releasedMs_ = nowMs;
            }
        }

        if (stable_ && !longFired_ && nowMs - pressedMs_ >= kHoldMs) {
            longFired_ = true;
            pendingTap_ = false; // holding never triggers a short tap
            return Action::Hold;
        }

        if (allowDoubleTap_ && pendingTap_ && !stable_ &&
            nowMs - releasedMs_ >= kDoubleTapMs) {
            pendingTap_ = false;
            return Action::Tap;
        }
        return Action::None;
    }

private:
    bool allowDoubleTap_ = false;
    bool raw_ = false;
    bool stable_ = false;
    bool pendingTap_ = false;
    bool longFired_ = false;
    uint32_t changedMs_ = 0;
    uint32_t pressedMs_ = 0;
    uint32_t releasedMs_ = 0;
};

} // namespace deskpet_button
