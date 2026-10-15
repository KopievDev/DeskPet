#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ButtonGesture.h"

using deskpet_button::Action;
using deskpet_button::Gesture;

int main() {
    Gesture user(true);
    user.begin(false, 0u);

    // A short USER press is emitted once, after the double-tap window.
    assert(user.poll(true, 100u) == Action::None);
    assert(user.poll(true, 134u) == Action::None);
    assert(user.poll(true, 135u) == Action::None); // debounced down
    assert(user.poll(false, 200u) == Action::None);
    assert(user.poll(false, 235u) == Action::None); // debounced up
    assert(user.poll(false, 554u) == Action::None);
    assert(user.poll(false, 555u) == Action::Tap);
    assert(user.poll(false, 600u) == Action::None);

    // Two quick taps are one DoubleTap, NEVER two single taps.
    user.begin(false, 0u);
    assert(user.poll(true, 100u) == Action::None);
    assert(user.poll(true, 135u) == Action::None);
    assert(user.poll(false, 200u) == Action::None);
    assert(user.poll(false, 235u) == Action::None);
    assert(user.poll(true, 300u) == Action::None);
    assert(user.poll(true, 335u) == Action::None);
    assert(user.poll(false, 410u) == Action::None);
    assert(user.poll(false, 445u) == Action::DoubleTap);
    assert(user.poll(false, 900u) == Action::None);

    // USER hold fires once while held; release must not produce a tap.
    user.begin(false, 0u);
    assert(user.poll(true, 100u) == Action::None);
    assert(user.poll(true, 135u) == Action::None);
    assert(user.poll(true, 984u) == Action::None);
    assert(user.poll(true, 985u) == Action::Hold);
    assert(user.poll(true, 1400u) == Action::None);
    assert(user.poll(false, 1500u) == Action::None);
    assert(user.poll(false, 1535u) == Action::None);
    assert(user.poll(false, 2000u) == Action::None);

    // Electrical chatter under 35ms never creates a gesture.
    user.begin(false, 0u);
    assert(user.poll(true, 100u) == Action::None);
    assert(user.poll(false, 116u) == Action::None);
    assert(user.poll(false, 300u) == Action::None);

    // BOOT has no double-tap delay; sleep/wake happens on short release.
    Gesture boot(false);
    boot.begin(false, 0u);
    assert(boot.poll(true, 100u) == Action::None);
    assert(boot.poll(true, 135u) == Action::None);
    assert(boot.poll(false, 200u) == Action::None);
    assert(boot.poll(false, 235u) == Action::Tap);
    assert(boot.poll(false, 900u) == Action::None);

    // Holding BOOT cycles the personality without toggling sleep on release.
    assert(boot.poll(true, 1000u) == Action::None);
    assert(boot.poll(true, 1035u) == Action::None);
    assert(boot.poll(true, 1885u) == Action::Hold);
    assert(boot.poll(true, 2400u) == Action::None);
    assert(boot.poll(false, 2600u) == Action::None);
    assert(boot.poll(false, 2635u) == Action::None);

    // GPIO0 can be held during reset; that held button must not generate
    // an accidental Hold or Tap during bootloader entry.
    boot.begin(true, 0u);
    assert(boot.poll(true, 2000u) == Action::None);
    assert(boot.poll(false, 2100u) == Action::None);
    assert(boot.poll(false, 2135u) == Action::None);
    assert(boot.poll(true, 2200u) == Action::None);
    assert(boot.poll(true, 2235u) == Action::None);
    assert(boot.poll(false, 2335u) == Action::None);
    assert(boot.poll(false, 2370u) == Action::Tap);

    // Signed millis rollover is never used: subtraction stays uint32_t.
    const uint32_t edge = UINT32_MAX - 100u;
    user.begin(false, edge);
    assert(user.poll(true, edge + 10u) == Action::None);
    assert(user.poll(true, edge + 45u) == Action::None);
    assert(user.poll(false, edge + 100u) == Action::None);
    assert(user.poll(false, edge + 135u) == Action::None);
    assert(user.poll(false, edge + 455u) == Action::Tap);

    puts("PASS: tap/double/hold, no double actions, bounce, BOOT priority, rollover");
    return 0;
}
