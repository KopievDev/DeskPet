#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "AccessoryWardrobe.h"

using namespace deskpet_accessories;

int main() {
    Wardrobe w;
    w.begin(1u);
    assert(w.mode() == Mode::Auto);
    assert(isEmpty(w.outfit()));
    w.tick(Emotion::Neutral, Personality::Calm, 78, 70, 65, false);
    assert(isEmpty(w.outfit()));
    w.tick(Emotion::Neutral, Personality::Calm, 25, 70, 65, false);
    assert(w.outfit().head == Item::Beanie);
    w.tick(Emotion::Neutral, Personality::Calm, 78, 20, 65, false);
    assert(w.outfit().head == Item::Beanie);
    w.tick(Emotion::Excited, Personality::Calm, 78, 70, 65, false);
    assert(w.outfit().head == Item::PartyHat);
    assert(w.outfit().face == Item::BowTie);
    w.tick(Emotion::Gremlin, Personality::Cheeky, 78, 70, 90, false);
    assert(w.outfit().eyes == Item::Glasses);
    assert(w.outfit().face == Item::Mustache);
    w.tick(Emotion::Nerd, Personality::Cheeky, 78, 70, 90, false);
    assert(w.outfit().eyes == Item::None); // built-in Nerd glasses
    w.tick(Emotion::Neutral, Personality::Playful, 78, 90, 65, false);
    assert(w.outfit().head == Item::PartyHat);
    assert(w.outfit().face == Item::BowTie);

    w.equip(Item::Beanie);
    w.equip(Item::Glasses);
    w.equip(Item::Mustache);
    assert(w.mode() == Mode::Manual);
    assert(w.manualOutfit().head == Item::Beanie);
    assert(w.manualOutfit().eyes == Item::Glasses);
    assert(w.manualOutfit().face == Item::Mustache);
    w.equip(Item::PartyHat);
    w.equip(Item::BowTie);
    assert(w.outfit().head == Item::PartyHat);
    assert(w.outfit().face == Item::BowTie);
    assert(w.outfit().eyes == Item::Glasses);

    w.tick(Emotion::Neutral, Personality::Calm, 0, 0, 0, false);
    assert(w.outfit().head == Item::PartyHat);
    w.tick(Emotion::Neutral, Personality::Calm, 0, 0, 0, true);
    assert(isEmpty(w.outfit())); // sleep hides without deleting
    assert(w.manualOutfit().eyes == Item::Glasses);
    w.tick(Emotion::Neutral, Personality::Calm, 0, 0, 0, false);
    assert(w.outfit().head == Item::PartyHat); // restored on wake

    w.setMode(Mode::Off);
    w.tick(Emotion::Excited, Personality::Cheeky, 100, 100, 100, false);
    assert(isEmpty(w.outfit()));
    w.setMode(Mode::Manual);
    w.tick(Emotion::Neutral, Personality::Calm, 0, 0, 0, false);
    assert(w.outfit().eyes == Item::Glasses);
    w.setMode(Mode::Auto);
    w.tick(Emotion::Sad, Personality::Calm, 80, 70, 70, false);
    assert(w.outfit().head == Item::Beanie);
    assert(w.manualOutfit().head == Item::PartyHat);
    w.setMode(Mode::Manual);
    w.tick(Emotion::Neutral, Personality::Calm, 80, 70, 70, false);
    assert(w.outfit().head == Item::PartyHat);

    w.clear();
    assert(isEmpty(w.manualOutfit()) && isEmpty(w.outfit()));
    assert(w.mode() == Mode::Manual);
    assert(slotFor(Item::Beanie) == Slot::Head);
    assert(slotFor(Item::Glasses) == Slot::Eyes);
    assert(slotFor(Item::Mustache) == Slot::Face);
    assert(slotFor(Item::BowTie) == Slot::Face);
    assert(slotFor(Item::PartyHat) == Slot::Head);
    assert(slotFor(Item::None) == Slot::Face);

    Wardrobe a, b;
    a.begin(12345u);
    b.begin(12345u);
    for (int i = 0; i < 200; ++i) {
        a.surprise();
        b.surprise();
        const Outfit one = a.manualOutfit();
        const Outfit two = b.manualOutfit();
        assert(!isEmpty(one)); // random outfits always show a prop
        assert(one.head == two.head);
        assert(one.eyes == two.eyes);
        assert(one.face == two.face);
        assert(one.head == Item::None || one.head == Item::Beanie ||
               one.head == Item::PartyHat);
        assert(one.eyes == Item::None || one.eyes == Item::Glasses);
        assert(one.face == Item::None || one.face == Item::Mustache ||
               one.face == Item::BowTie);
    }
    assert(itemName(Item::PartyHat)[0] == 'p');
    assert(modeName(Mode::Off)[0] == 'o');
    puts("PASS: wardrobe host tests");
    return 0;
}
