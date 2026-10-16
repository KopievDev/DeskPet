#pragma once

#include <stdint.h>

#include "Emotion.h"
#include "Personality.h"

// Tiny RAM-only wardrobe. Each accessory belongs to exactly one visual slot.
// No NVS, heap allocation, global random(), timers, or external dependencies.
namespace deskpet_accessories {

enum class Item : uint8_t {
    None, Beanie, PartyHat, Glasses, Mustache, BowTie
};
enum class Mode : uint8_t { Manual, Auto, Off };
enum class Slot : uint8_t { Head, Eyes, Face };

struct Outfit {
    Item head;
    Item eyes;
    Item face;
};

inline Outfit emptyOutfit() {
    return {Item::None, Item::None, Item::None};
}

inline const char* itemName(Item item) {
    switch (item) {
        case Item::None: return "none";
        case Item::Beanie: return "beanie";
        case Item::PartyHat: return "partyhat";
        case Item::Glasses: return "glasses";
        case Item::Mustache: return "mustache";
        case Item::BowTie: return "bowtie";
    }
    return "none";
}

inline const char* modeName(Mode mode) {
    switch (mode) {
        case Mode::Manual: return "manual";
        case Mode::Auto: return "auto";
        case Mode::Off: return "off";
    }
    return "off";
}

inline Slot slotFor(Item item) {
    switch (item) {
        case Item::Beanie:
        case Item::PartyHat: return Slot::Head;
        case Item::Glasses: return Slot::Eyes;
        case Item::None:
        case Item::Mustache:
        case Item::BowTie: return Slot::Face;
    }
    return Slot::Face;
}

inline bool isEmpty(Outfit outfit) {
    return outfit.head == Item::None && outfit.eyes == Item::None &&
           outfit.face == Item::None;
}

// Auto mode reacts only to game state, never overwrites a manual outfit.
inline Outfit automaticOutfit(Emotion emotion, Personality personality,
                             uint8_t energy, uint8_t mood,
                             uint8_t attention) {
    Outfit outfit = emptyOutfit();
    if (energy <= 25u || mood <= 20u || emotion == Emotion::Sleepy ||
        emotion == Emotion::Sad) {
        outfit.head = Item::Beanie;
    } else if (emotion == Emotion::Excited || emotion == Emotion::Happy ||
               (personality == Personality::Playful && mood >= 80u)) {
        outfit.head = Item::PartyHat;
    }

    // Nerd already has its own animated spectacles in ComicSkits.
    if (emotion != Emotion::Nerd &&
        (emotion == Emotion::Smug ||
         (personality == Personality::Cheeky && attention >= 30u))) {
        outfit.eyes = Item::Glasses;
    }

    if (emotion == Emotion::Gremlin ||
        (personality == Personality::Cheeky && attention >= 75u)) {
        outfit.face = Item::Mustache;
    } else if (emotion == Emotion::Love || emotion == Emotion::Excited ||
               (personality == Personality::Playful && mood >= 75u)) {
        outfit.face = Item::BowTie;
    }
    return outfit;
}

class Wardrobe {
public:
    void begin(uint32_t seed) {
        rng_ = seed == 0u ? 0xD35CA7u : seed;
        mode_ = Mode::Auto;
        manual_ = emptyOutfit();
        visible_ = emptyOutfit();
    }

    Mode mode() const { return mode_; }
    Outfit outfit() const { return visible_; }
    Outfit manualOutfit() const { return manual_; }

    void setMode(Mode mode) {
        mode_ = mode;
        visible_ = mode == Mode::Manual ? manual_ : emptyOutfit();
    }

    void clear() {
        manual_ = emptyOutfit();
        setMode(Mode::Manual);
    }

    void equip(Item item) {
        if (item == Item::None) {
            clear();
            return;
        }
        const Slot slot = slotFor(item);
        if (slot == Slot::Head) manual_.head = item;
        else if (slot == Slot::Eyes) manual_.eyes = item;
        else manual_.face = item;
        setMode(Mode::Manual);
    }

    // Separate PRNG; doesn't change eye movement, blinks or ambience RNG.
    void surprise() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        const uint32_t value = rng_;
        manual_.head = (value & 3u) == 0u ? Item::Beanie :
                       (value & 3u) == 1u ? Item::PartyHat : Item::None;
        manual_.eyes = (value & 4u) != 0u ? Item::Glasses : Item::None;
        manual_.face = (value & 8u) != 0u ? Item::Mustache :
                       (value & 16u) != 0u ? Item::BowTie : Item::None;
        if (isEmpty(manual_)) manual_.face = Item::BowTie;
        setMode(Mode::Manual);
    }

    void tick(Emotion emotion, Personality personality,
              uint8_t energy, uint8_t mood, uint8_t attention,
              bool sleeping) {
        if (sleeping || mode_ == Mode::Off) {
            visible_ = emptyOutfit();
        } else if (mode_ == Mode::Manual) {
            visible_ = manual_;
        } else {
            visible_ = automaticOutfit(emotion, personality, energy, mood,
                                       attention);
        }
    }

private:
    Mode mode_ = Mode::Auto;
    Outfit manual_ = emptyOutfit();
    Outfit visible_ = emptyOutfit();
    uint32_t rng_ = 0xD35CA7u;
};

} // namespace deskpet_accessories
