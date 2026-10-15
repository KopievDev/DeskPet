#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "IdleBehavior.h"

static uint32_t firstGesture(IdleBehavior& idle, uint32_t now) {
    // The first event must occur after a quiet delay, never immediately.
    assert(!idle.tick(now, true));
    for (uint32_t ms = 1; ms <= 27000; ms += 50) {
        if (idle.tick(now + ms, true)) {
            assert(idle.isActive());
            assert(idle.gesture() != IdleGesture::None);
            assert(ms >= 14000 && ms <= 26050);
            return now + ms;
        }
    }
    assert(false && "Idle gesture never started");
    return now;
}

int main() {
    IdleBehavior idle;
    idle.begin(1000, 7);
    assert(idle.enabled());
    assert(!idle.isActive());

    // Deterministic first gesture with a fixed seed.
    const uint32_t start = firstGesture(idle, 1000);
    const IdleGesture gesture = idle.gesture();
    assert(IdleBehavior::duration(gesture) >= 1100);
    assert(!idle.tick(start + IdleBehavior::duration(gesture) - 1, true));
    assert(idle.tick(start + IdleBehavior::duration(gesture), true));
    assert(!idle.isActive());
    assert(idle.gesture() == IdleGesture::None);
    assert(!idle.tick(start + IdleBehavior::duration(gesture) + 1, true));

    // Another gesture is scheduled after a full new quiet interval.
    const uint32_t finishedAt = start + IdleBehavior::duration(gesture);
    uint32_t nextStart = 0;
    for (uint32_t delta = 50; delta <= 26100; delta += 50) {
        if (idle.tick(finishedAt + delta, true)) {
            nextStart = finishedAt + delta;
            assert(idle.isActive());
            assert(delta >= 14000 && delta <= 26100);
            break;
        }
    }
    assert(nextStart != 0);
    assert(idle.tick(nextStart + 100, false)); // build/demo/sleep interruption
    assert(idle.gesture() == IdleGesture::None);
    assert(!idle.tick(nextStart + 150, false));
    assert(!idle.tick(nextStart + 200, true)); // resume starts a fresh delay

    // Explicit OFF clears the active gesture and it stays off.
    idle.setEnabled(false, nextStart + 210);
    assert(!idle.enabled());
    assert(idle.gesture() == IdleGesture::None);
    assert(!idle.tick(nextStart + 50000, true));
    idle.setEnabled(true, nextStart + 50100);
    assert(idle.enabled());
    assert(!idle.tick(nextStart + 50100, true));

    // All 7 gestures are reachable in a long deterministic sequence,
    // without extra heap allocations or elapsed-time drift.
    IdleBehavior coverage;
    coverage.begin(42, 0x1234ABCDu);
    bool seen[8] = {};
    uint32_t time = 42;
    coverage.tick(time, true);
    for (int i = 0; i < 120; ++i) {
        bool began = false;
        for (uint32_t ms = 100; ms <= 27000; ms += 100) {
            if (coverage.tick(time + ms, true) && coverage.isActive()) {
                time += ms;
                seen[static_cast<uint8_t>(coverage.gesture())] = true;
                began = true;
                break;
            }
        }
        assert(began);
        const uint32_t duration = IdleBehavior::duration(coverage.gesture());
        time += duration;
        assert(coverage.tick(time, true));
        assert(!coverage.isActive());
    }
    for (int i = 1; i <= 7; ++i) assert(seen[i]);

    // millis() rollover: unsigned elapsed and signed deadline comparisons
    // remain well-defined while the time horizon is under 2^31 ms.
    IdleBehavior wrap;
    const uint32_t beforeWrap = UINT32_MAX - 800;
    wrap.begin(beforeWrap, 77);
    uint32_t wrappedStart = firstGesture(wrap, beforeWrap);
    assert(wrap.isActive());
    const uint32_t dur = IdleBehavior::duration(wrap.gesture());
    assert(wrap.tick(wrappedStart + dur, true));
    assert(!wrap.isActive());

    puts("PASS: ambient scheduler cadence, all 7 gestures, cancellations, mode and wrap-around");
    return 0;
}
