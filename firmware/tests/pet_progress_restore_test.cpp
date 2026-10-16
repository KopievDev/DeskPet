#include <assert.h>
#include <stdio.h>

#include "PetBond.h"
#include "PetNeeds.h"
#include "PetFinds.h"
#include "AccessoryWardrobe.h"
#include "PetProgress.h"

using namespace deskpet_progress;
using deskpet_accessories::Item;
using deskpet_accessories::Mode;

int main() {
    PetBond bond;
    deskpet_needs::Controller needs;
    deskpet_finds::Collection finds;
    deskpet_accessories::Wardrobe wardrobe;
    bond.begin(100u, 11u);
    needs.begin(100u);
    finds.begin(12u);
    wardrobe.begin(13u);

    Snapshot save;
    save.bond = 91u;
    save.energy = 15u;
    save.mood = 80u;
    save.attention = 32u;
    save.owned = 3u;
    save.play = 2u;
    save.pet = 4u;
    save.builds = 2u;
    save.wardrobeMode = Mode::Manual;
    save.outfit = {Item::Crown, Item::Shades, Item::BowTie};
    save.personality = Personality::Playful;
    save.needsEnabled = false;
    save.bondEnabled = false;
    assert(valid(save));

    uint8_t encoded[kWireSize];
    assert(encode(save, 42u, encoded));
    Snapshot decoded;
    uint32_t seq = 0u;
    assert(decode(encoded, sizeof(encoded), decoded, seq));

    const uint32_t rebootMs = 8000u;
    bond.restore(decoded.bond, rebootMs);
    bond.setEnabled(decoded.bondEnabled, rebootMs);
    needs.restore(decoded.energy, decoded.mood, decoded.attention,
                  decoded.needsEnabled, rebootMs);
    finds.restore({decoded.owned, decoded.play, decoded.pet, decoded.builds},
                  rebootMs);
    wardrobe.restore(decoded.wardrobeMode, decoded.outfit);
    assert(bond.score() == 91u && !bond.enabled());
    assert(needs.energy() == 15u && needs.mood() == 80u &&
           needs.attention() == 32u && !needs.enabled());
    assert(finds.owns(deskpet_finds::Prize::Crown));
    assert(finds.owns(deskpet_finds::Prize::Shades));
    assert(!finds.owns(deskpet_finds::Prize::StarPin));
    assert(finds.playProgress() == 2u && finds.petProgress() == 4u &&
           finds.buildProgress() == 2u);
    assert(wardrobe.mode() == Mode::Manual);
    assert(wardrobe.manualOutfit().head == Item::Crown);
    assert(wardrobe.manualOutfit().eyes == Item::Shades);
    assert(wardrobe.manualOutfit().face == Item::BowTie);

    // Power-off must not count as lost energy or lost affection.
    needs.advance(rebootMs + 1000u, false);
    assert(needs.energy() == 15u);
    Emotion cameo = Emotion::Neutral;
    assert(!bond.tick(rebootMs + 1000u, true, false,
                      Personality::Playful, cameo));
    assert(bond.score() == 91u);
    // Restoring an earned item restarts cooldown to resist reboot farming.
    assert(finds.reward(deskpet_finds::Action::BuildSuccess,
                        rebootMs + 100u) == deskpet_finds::Prize::None);
    wardrobe.tick(Emotion::Neutral, Personality::Playful, 15, 80, 32, true);
    assert(deskpet_accessories::isEmpty(wardrobe.outfit()));
    wardrobe.tick(Emotion::Neutral, Personality::Playful, 15, 80, 32, false);
    assert(wardrobe.outfit().head == Item::Crown);

    puts("PASS: gameplay controllers restore from validated save");
    return 0;
}
