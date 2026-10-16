#pragma once

#include <stdint.h>
#include <stddef.h>

#include "AccessoryWardrobe.h"
#include "Personality.h"

// Versioned, endian-stable game checkpoint. No Arduino or NVS dependency.
// Deliberately excludes sleep, reactions, sass, and host activity state.
namespace deskpet_progress {

struct Snapshot {
    uint8_t bond = 50u;
    uint8_t energy = 78u;
    uint8_t mood = 70u;
    uint8_t attention = 65u;
    uint8_t owned = 0u;
    uint8_t play = 0u;
    uint8_t pet = 0u;
    uint8_t builds = 0u;
    deskpet_accessories::Mode wardrobeMode = deskpet_accessories::Mode::Auto;
    deskpet_accessories::Outfit outfit = deskpet_accessories::emptyOutfit();
    Personality personality = Personality::Calm;
    bool needsEnabled = true;
    bool bondEnabled = true;
};

static constexpr size_t kWireSize = 28u;
static constexpr uint8_t kVersion = 1u;
static constexpr uint8_t kPayloadLength = 14u;
static constexpr uint32_t kDebounceMs = 15000u;
static constexpr uint32_t kMinWriteIntervalMs = 60000u;
static constexpr uint32_t kRetryMs = 5000u;

inline bool equal(const Snapshot& a, const Snapshot& b) {
    return a.bond == b.bond && a.energy == b.energy &&
           a.mood == b.mood && a.attention == b.attention &&
           a.owned == b.owned && a.play == b.play &&
           a.pet == b.pet && a.builds == b.builds &&
           a.wardrobeMode == b.wardrobeMode &&
           a.outfit.head == b.outfit.head &&
           a.outfit.eyes == b.outfit.eyes &&
           a.outfit.face == b.outfit.face &&
           a.personality == b.personality &&
           a.needsEnabled == b.needsEnabled &&
           a.bondEnabled == b.bondEnabled;
}

inline bool valid(const Snapshot& s) {
    using deskpet_accessories::Item;
    using deskpet_accessories::Mode;
    if (s.bond > 100u || s.energy > 100u || s.mood > 100u ||
        s.attention > 100u || (s.owned & ~7u) != 0u ||
        s.play > 3u || s.pet > 4u || s.builds > 2u ||
        static_cast<uint8_t>(s.wardrobeMode) > static_cast<uint8_t>(Mode::Off) ||
        static_cast<uint8_t>(s.personality) >
            static_cast<uint8_t>(Personality::Cheeky)) return false;

    const Item head = s.outfit.head;
    const Item eyes = s.outfit.eyes;
    const Item face = s.outfit.face;
    if ((head != Item::None && head != Item::Beanie &&
         head != Item::PartyHat && head != Item::Crown) ||
        (eyes != Item::None && eyes != Item::Glasses && eyes != Item::Shades) ||
        (face != Item::None && face != Item::Mustache &&
         face != Item::BowTie && face != Item::StarPin)) return false;
    if ((head == Item::Crown && !(s.owned & 1u)) ||
        (eyes == Item::Shades && !(s.owned & 2u)) ||
        (face == Item::StarPin && !(s.owned & 4u))) return false;
    return true;
}

inline uint32_t checksum(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return ~crc;
}

inline void write32(uint8_t* target, uint32_t value) {
    for (uint8_t i = 0; i < 4u; ++i)
        target[i] = static_cast<uint8_t>(value >> (i * 8u));
}

inline uint32_t read32(const uint8_t* source) {
    return static_cast<uint32_t>(source[0]) |
           (static_cast<uint32_t>(source[1]) << 8) |
           (static_cast<uint32_t>(source[2]) << 16) |
           (static_cast<uint32_t>(source[3]) << 24);
}

inline bool encode(const Snapshot& s, uint32_t sequence,
                   uint8_t (&out)[kWireSize]) {
    if (!valid(s)) return false;
    out[0] = 'D'; out[1] = 'P'; out[2] = 'E'; out[3] = 'T';
    out[4] = kVersion;
    out[5] = kPayloadLength;
    out[6] = s.bond;
    out[7] = s.energy;
    out[8] = s.mood;
    out[9] = s.attention;
    out[10] = s.owned;
    out[11] = s.play;
    out[12] = s.pet;
    out[13] = s.builds;
    out[14] = static_cast<uint8_t>(s.wardrobeMode);
    out[15] = static_cast<uint8_t>(s.outfit.head);
    out[16] = static_cast<uint8_t>(s.outfit.eyes);
    out[17] = static_cast<uint8_t>(s.outfit.face);
    out[18] = static_cast<uint8_t>(s.personality);
    out[19] = (s.needsEnabled ? 1u : 0u) | (s.bondEnabled ? 2u : 0u);
    write32(out + 20, sequence);
    write32(out + 24, checksum(out, 24));
    return true;
}

inline bool decode(const uint8_t* bytes, size_t length, Snapshot& result,
                   uint32_t& sequence) {
    if (!bytes || length != kWireSize ||
        bytes[0] != 'D' || bytes[1] != 'P' ||
        bytes[2] != 'E' || bytes[3] != 'T' ||
        bytes[4] != kVersion || bytes[5] != kPayloadLength ||
        checksum(bytes, 24) != read32(bytes + 24) ||
        (bytes[19] & ~3u) != 0u) return false;
    Snapshot candidate;
    candidate.bond = bytes[6];
    candidate.energy = bytes[7];
    candidate.mood = bytes[8];
    candidate.attention = bytes[9];
    candidate.owned = bytes[10];
    candidate.play = bytes[11];
    candidate.pet = bytes[12];
    candidate.builds = bytes[13];
    candidate.wardrobeMode = static_cast<deskpet_accessories::Mode>(bytes[14]);
    candidate.outfit.head = static_cast<deskpet_accessories::Item>(bytes[15]);
    candidate.outfit.eyes = static_cast<deskpet_accessories::Item>(bytes[16]);
    candidate.outfit.face = static_cast<deskpet_accessories::Item>(bytes[17]);
    candidate.personality = static_cast<Personality>(bytes[18]);
    candidate.needsEnabled = (bytes[19] & 1u) != 0u;
    candidate.bondEnabled = (bytes[19] & 2u) != 0u;
    if (!valid(candidate)) return false;
    result = candidate;
    sequence = read32(bytes + 20);
    return true;
}

inline bool newer(uint32_t a, uint32_t b) {
    return a != b && static_cast<uint32_t>(a - b) < 0x80000000u;
}

// A pure scheduling policy: observe changes in loop(), write no sooner
// than 15 s after first change, and at most once per 60 s thereafter.
// Persisting the snapshot is the caller's responsibility.
class Journal {
public:
    void begin(const Snapshot& baseline, uint32_t nowMs, bool loaded) {
        baseline_ = baseline;
        dirty_ = false;
        loaded_ = loaded;
        writtenThisBoot_ = false;
        retryPending_ = false;
        changedAt_ = nowMs;
        writtenAt_ = nowMs;
        attemptedAt_ = nowMs;
    }

    void observe(const Snapshot& value, uint32_t nowMs) {
        if (equal(value, baseline_)) {
            dirty_ = false;
            return;
        }
        if (!dirty_) {
            dirty_ = true;
            changedAt_ = nowMs;
        }
    }

    bool shouldWrite(uint32_t nowMs) const {
        return dirty_ &&
               static_cast<uint32_t>(nowMs - changedAt_) >= kDebounceMs &&
               (!writtenThisBoot_ ||
                static_cast<uint32_t>(nowMs - writtenAt_) >=
                    kMinWriteIntervalMs) &&
               (!retryPending_ ||
                static_cast<uint32_t>(nowMs - attemptedAt_) >= kRetryMs);
    }

    bool dirty() const { return dirty_; }
    bool loaded() const { return loaded_; }

    void wrote(const Snapshot& value, uint32_t nowMs) {
        baseline_ = value;
        dirty_ = false;
        loaded_ = true;
        writtenThisBoot_ = true;
        retryPending_ = false;
        writtenAt_ = nowMs;
    }

    void failed(uint32_t nowMs) {
        retryPending_ = true;
        attemptedAt_ = nowMs;
    }

private:
    Snapshot baseline_;
    uint32_t changedAt_ = 0u;
    uint32_t writtenAt_ = 0u;
    uint32_t attemptedAt_ = 0u;
    bool dirty_ = false;
    bool loaded_ = false;
    bool writtenThisBoot_ = false;
    bool retryPending_ = false;
};

} // namespace deskpet_progress
