#pragma once

#include <Preferences.h>
#include "PetProgress.h"

// ESP32-only boundary around a portable, validated save format.
// Two independently committed NVS keys alternate: a failed write leaves
// the previous valid generation recoverable.
namespace deskpet_progress {

class Storage {
public:
    bool begin() {
        ready_ = prefs_.begin("deskpet", false);
        return ready_;
    }

    bool ready() const { return ready_; }
    bool recovered() const { return recovered_; }

    bool load(Snapshot& value) {
        recovered_ = false;
        if (!ready_) return false;
        Snapshot a, b;
        uint32_t seqA = 0u, seqB = 0u;
        const bool validA = read("saveA", a, seqA);
        const bool validB = read("saveB", b, seqB);
        if (!validA && !validB) {
            nextSequence_ = 0u;
            nextA_ = true;
            return false;
        }
        const bool chooseA = validA && (!validB || newer(seqA, seqB));
        value = chooseA ? a : b;
        nextSequence_ = (chooseA ? seqA : seqB) + 1u;
        nextA_ = !chooseA;
        recovered_ = true;
        return true;
    }

    bool write(const Snapshot& value) {
        if (!ready_) return false;
        uint8_t bytes[kWireSize];
        if (!encode(value, nextSequence_, bytes)) return false;
        const char* key = nextA_ ? "saveA" : "saveB";
        if (prefs_.putBytes(key, bytes, kWireSize) != kWireSize)
            return false;
        nextA_ = !nextA_;
        ++nextSequence_;
        recovered_ = true;
        return true;
    }

    bool clear() {
        if (!ready_) return false;
        // remove() reports false for absent keys, so verify postcondition.
        prefs_.remove("saveA");
        prefs_.remove("saveB");
        const bool empty = prefs_.getBytesLength("saveA") == 0u &&
                           prefs_.getBytesLength("saveB") == 0u;
        if (empty) {
            nextA_ = true;
            nextSequence_ = 0u;
            recovered_ = false;
        }
        return empty;
    }

private:
    bool read(const char* key, Snapshot& result, uint32_t& sequence) {
        if (prefs_.getBytesLength(key) != kWireSize) return false;
        uint8_t bytes[kWireSize];
        if (prefs_.getBytes(key, bytes, kWireSize) != kWireSize) return false;
        return decode(bytes, kWireSize, result, sequence);
    }

    Preferences prefs_;
    uint32_t nextSequence_ = 0u;
    bool nextA_ = true;
    bool ready_ = false;
    bool recovered_ = false;
};

} // namespace deskpet_progress
