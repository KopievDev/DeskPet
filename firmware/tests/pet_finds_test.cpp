#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "PetFinds.h"

using namespace deskpet_finds;

int main() {
    Collection c;
    c.begin(100u);
    assert(c.rareCount() == 0u);
    assert(c.owns(Prize::None));
    assert(!c.owns(Prize::Crown));
    assert(!c.owns(Prize::Shades));
    assert(!c.owns(Prize::StarPin));
    assert(!c.grant(Prize::None));
    assert(c.reward(Action::BuildSuccess, 100u) == Prize::None);
    assert(c.reward(Action::BuildSuccess, 101u) == Prize::None);
    const Prize first = c.reward(Action::BuildSuccess, 102u);
    assert(first != Prize::None && c.owns(first));
    assert(c.rareCount() == 1u);

    for (int i = 0; i < 6; ++i)
        assert(c.reward(Action::BuildSuccess, 200u+i) == Prize::None);
    assert(c.rareCount() == 1u); // cooldown blocks Serial-spam
    for (int i = 0; i < 3; ++i)
        assert(c.reward(Action::Play, 61100u+i) == Prize::None);
    const Prize second = c.reward(Action::Play, 61103u);
    assert(second != Prize::None && second != first);
    for (int i = 0; i < 4; ++i)
        assert(c.reward(Action::Pet, 122110u+i) == Prize::None);
    const Prize third = c.reward(Action::Pet, 122114u);
    assert(third != Prize::None && third != first && third != second);
    assert(c.rareCount() == 3u);
    assert(c.reward(Action::Play, 999999u) == Prize::None);
    assert(!c.grant(first));

    c.reset();
    assert(c.rareCount() == 0u && c.playProgress() == 0u);
    assert(c.buildProgress() == 0u && c.petProgress() == 0u);
    assert(c.grant(Prize::Crown));
    assert(c.owns(Prize::Crown) && c.rareCount() == 1u);
    assert(!c.grant(Prize::Crown));

    Collection a, b;
    a.begin(42u);
    b.begin(42u);
    for (int cycle = 0; cycle < 3; ++cycle) {
        Prize x = Prize::None, y = Prize::None;
        for (int n = 0; n < 3; ++n) {
            x = a.reward(Action::BuildSuccess, cycle*61000u+n);
            y = b.reward(Action::BuildSuccess, cycle*61000u+n);
        }
        assert(x == y && x != Prize::None);
    }
    assert(a.rareCount() == 3u);

    Collection wrap;
    wrap.begin(91u);
    for (int n = 0; n < 3; ++n)
        wrap.reward(Action::BuildSuccess, 0xFFFF0000u+n);
    assert(wrap.rareCount() == 1u);
    for (int n = 0; n < 4; ++n)
        wrap.reward(Action::Play, 0x00020000u+n);
    assert(wrap.rareCount() == 2u);

    assert(prizeName(Prize::StarPin)[0] == 's');
    puts("PASS: collectible rare finds and cooldown");
    return 0;
}
