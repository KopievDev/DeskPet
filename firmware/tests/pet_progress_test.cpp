#include <assert.h>
#include <stdio.h>
#include "PetProgress.h"

using namespace deskpet_progress;
using deskpet_accessories::Item;
using deskpet_accessories::Mode;

int main() {
    Snapshot defaults;
    assert(valid(defaults));
    uint8_t buffer[kWireSize];
    assert(encode(defaults, 7u, buffer));
    assert(buffer[0] == 'D' && buffer[4] == 1u && buffer[5] == 14u);
    Snapshot decoded;
    uint32_t sequence = 0u;
    assert(decode(buffer, sizeof(buffer), decoded, sequence));
    assert(equal(defaults, decoded) && sequence == 7u);
    assert(!decode(buffer, sizeof(buffer) - 1u, decoded, sequence));
    assert(!decode(nullptr, sizeof(buffer), decoded, sequence));

    Snapshot full;
    full.bond = 100u;
    full.energy = 0u;
    full.mood = 12u;
    full.attention = 99u;
    full.owned = 7u;
    full.play = 3u;
    full.pet = 4u;
    full.builds = 2u;
    full.wardrobeMode = Mode::Manual;
    full.outfit = {Item::Crown, Item::Shades, Item::StarPin};
    full.personality = Personality::Cheeky;
    full.needsEnabled = false;
    full.bondEnabled = false;
    assert(encode(full, 0xFFFFFFFFu, buffer));
    assert(decode(buffer, sizeof(buffer), decoded, sequence));
    assert(equal(decoded, full) && sequence == 0xFFFFFFFFu);
    assert(newer(0u, 0xFFFFFFFFu));
    assert(!newer(0xFFFFFFFFu, 0u));
    assert(!newer(3u, 3u));
    assert(preferA(true, 5u, false, 99u));
    assert(!preferA(false, 5u, true, 99u));
    assert(preferA(true, 0u, true, 0xFFFFFFFFu));
    assert(!preferA(true, 42u, true, 43u));

    // Bit errors, wrong version, unexpected sizes, valid CRC with
    // semantically impossible inventory or progress are all rejected.
    buffer[9] ^= 1u;
    assert(!decode(buffer, sizeof(buffer), decoded, sequence));
    buffer[9] ^= 1u;
    buffer[4] = 2u;
    write32(buffer + 24, checksum(buffer, 24u));
    assert(!decode(buffer, sizeof(buffer), decoded, sequence));
    buffer[4] = 1u;
    buffer[10] = 0u; // rare outfit without ownership
    write32(buffer + 24, checksum(buffer, 24u));
    assert(!decode(buffer, sizeof(buffer), decoded, sequence));
    buffer[10] = 7u;
    buffer[11] = 4u; // progress reaches 4 only transiently
    write32(buffer + 24, checksum(buffer, 24u));
    assert(!decode(buffer, sizeof(buffer), decoded, sequence));
    buffer[11] = 3u;
    buffer[14] = 255u; // invalid mode
    write32(buffer + 24, checksum(buffer, 24u));
    assert(!decode(buffer, sizeof(buffer), decoded, sequence));
    assert(encode(full, 8u, buffer));

    Snapshot invalid = defaults;
    invalid.energy = 101u;
    assert(!encode(invalid, 0u, buffer));
    invalid = defaults;
    invalid.outfit.eyes = Item::Crown;
    assert(!valid(invalid));
    invalid = defaults;
    invalid.outfit.head = Item::Crown;
    assert(!valid(invalid));
    invalid.owned = 1u;
    assert(valid(invalid));

    Journal journal;
    journal.begin(defaults, 100u, false);
    assert(!journal.shouldWrite(30000u));
    Snapshot changed = defaults;
    changed.mood = 65u;
    journal.observe(changed, 500u);
    assert(journal.dirty());
    assert(!journal.shouldWrite(15499u));
    assert(journal.shouldWrite(15500u));
    journal.failed(15500u);
    assert(!journal.shouldWrite(19500u));
    assert(journal.shouldWrite(20500u));
    journal.wrote(changed, 20500u);
    assert(!journal.dirty() && journal.loaded());
    changed.mood = 64u;
    journal.observe(changed, 21000u);
    assert(!journal.shouldWrite(60000u));
    assert(journal.shouldWrite(80500u));
    journal.observe(defaults, 80501u);
    assert(journal.dirty()); // differs from saved mood 65
    changed.mood = 65u;
    journal.observe(changed, 80502u);
    assert(!journal.dirty());
    assert(!journal.shouldWrite(999999u));

    journal.begin(defaults, 0xFFFFFF00u, true);
    changed = defaults;
    changed.bond = 60u;
    journal.observe(changed, 0xFFFFFFF0u);
    assert(!journal.shouldWrite(0x00003000u));
    assert(journal.shouldWrite(0x00003B00u));
    journal.wrote(changed, 0x00003B00u);
    assert(!journal.shouldWrite(0x0000FFFFu));

    puts("PASS: progress codec, CRC, version, slot sequencing and throttled writes");
    return 0;
}
