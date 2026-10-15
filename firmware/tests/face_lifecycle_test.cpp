#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "FaceLifecycle.h"

using deskpet_lifecycle::Phase;
using deskpet_lifecycle::Timeline;

static bool closeTo(float actual, float expected, float tolerance = 0.003f) {
    return fabsf(actual - expected) <= tolerance;
}

int main() {
    Timeline motion;
    motion.begin(1000u);
    assert(motion.phase() == Phase::Booting);
    assert(!motion.sleeping());
    assert(closeTo(motion.openness(1000u), 0.055f));
    assert(closeTo(motion.openness(1230u), 0.055f));
    assert(motion.openness(1710u) > 0.5f);
    assert(closeTo(motion.openness(2190u), 1.0f));
    motion.advance(2599u);
    assert(motion.phase() == Phase::Booting);
    motion.advance(2600u);
    assert(motion.phase() == Phase::Awake);
    assert(closeTo(motion.openness(2600u), 1.0f));

    // The screen goes to sleep gradually, but the public sleeping flag
    // changes immediately so build events never wake the pet.
    motion.sleep(3000u);
    assert(motion.sleeping());
    assert(motion.phase() == Phase::FallingAsleep);
    assert(closeTo(motion.openness(3000u), 1.0f));
    assert(motion.openness(3400u) < 0.6f);
    assert(motion.openness(3400u) > 0.25f);
    motion.advance(3850u);
    assert(motion.phase() == Phase::Asleep);
    assert(closeTo(motion.openness(4000u), 0.055f));

    // Wake preserves whatever manual emotion the renderer already holds.
    motion.wake(4400u);
    assert(!motion.sleeping());
    assert(closeTo(motion.openness(4400u), 0.055f));
    assert(motion.openness(4925u) > 0.5f);
    motion.advance(5450u);
    assert(motion.phase() == Phase::Awake);
    assert(closeTo(motion.openness(5450u), 1.0f));

    // Sleep interrupted by wake: no jump to 0 or 1 at reversal.
    motion.sleep(6000u);
    const float halfway = motion.openness(6320u);
    motion.wake(6320u);
    assert(closeTo(motion.openness(6320u), halfway));
    assert(motion.openness(6720u) > halfway);

    // Wake interrupted by sleep: again preserve the visible opening.
    const float partOpen = motion.openness(6880u);
    motion.sleep(6880u);
    assert(closeTo(motion.openness(6880u), partOpen));
    motion.advance(7730u);
    assert(motion.sleeping());
    assert(closeTo(motion.openness(7730u), 0.055f));

    // Explicit emotion selection interrupts a boot animation gracefully.
    motion.begin(9000u);
    const float bootOpening = motion.openness(9510u);
    motion.interruptBoot(9510u);
    assert(motion.phase() == Phase::Waking);
    assert(closeTo(motion.openness(9510u), bootOpening));
    motion.advance(10560u);
    assert(motion.phase() == Phase::Awake);

    // Rollover safety for millis() subtraction (boot and wake).
    const uint32_t nearWrap = UINT32_MAX - 300u;
    motion.begin(nearWrap);
    motion.advance(nearWrap + 1700u);
    assert(motion.phase() == Phase::Awake);
    motion.sleep(nearWrap + 1800u);
    motion.advance(nearWrap + 2650u);
    assert(motion.phase() == Phase::Asleep);
    motion.wake(nearWrap + 2700u);
    motion.advance(nearWrap + 3750u);
    assert(motion.phase() == Phase::Awake);

    // Exercise monotonic opening/closing at 33ms frame cadence.
    motion.begin(0u);
    float prev = 0.0f;
    for (uint32_t time = 0; time <= 1600u; time += 33u) {
        const float now = motion.openness(time);
        assert(now >= prev - 0.0001f);
        assert(now <= 1.0f && now >= 0.055f);
        prev = now;
    }
    motion.advance(1600u);
    motion.sleep(1700u);
    prev = 1.0f;
    for (uint32_t time = 1700u; time <= 2550u; time += 33u) {
        const float now = motion.openness(time);
        assert(now <= prev + 0.0001f);
        prev = now;
    }

    puts("PASS: nonblocking boot, sleeping, wake, interruptions and millis rollover");
    return 0;
}
