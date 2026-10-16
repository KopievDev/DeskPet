#pragma once

#include <stdint.h>

// Collectible rare accessories. Host-testable C++11, RAM only.
// No blocking waits, global random(), NVS, heap, or network.
namespace deskpet_finds {

enum class Prize : uint8_t { None, Crown, Shades, StarPin };
enum class Action : uint8_t { Play, Pet, BuildSuccess };

inline const char* prizeName(Prize prize) {
    switch (prize) {
        case Prize::None: return "none";
        case Prize::Crown: return "crown";
        case Prize::Shades: return "shades";
        case Prize::StarPin: return "starpin";
    }
    return "none";
}

class Collection {
public:
    static constexpr uint32_t kCooldownMs = 60000u;

    void begin(uint32_t seed) {
        rng_ = seed == 0u ? 0xC01EC7u : seed;
        reset();
    }

    void reset() {
        owned_ = 0u; // The five common wardrobe props remain unlocked.
        playProgress_ = petProgress_ = buildProgress_ = 0u;
        lastFindMs_ = 0u;
        hasFound_ = false;
    }

    struct State {
        uint8_t owned;
        uint8_t play;
        uint8_t pet;
        uint8_t builds;
    };

    State state() const {
        return {owned_, playProgress_, petProgress_, buildProgress_};
    }

    // Persistence adapter: previously earned items survive a restart.
    // Cooldown is reset conservatively to prevent reboot reward farming.
    void restore(State value, uint32_t nowMs) {
        owned_ = value.owned & 7u;
        playProgress_ = value.play > 3u ? 0u : value.play;
        petProgress_ = value.pet > 4u ? 0u : value.pet;
        buildProgress_ = value.builds > 2u ? 0u : value.builds;
        hasFound_ = owned_ != 0u;
        lastFindMs_ = nowMs;
    }

    bool owns(Prize prize) const {
        return prize == Prize::None ||
               (owned_ & mask(prize)) != 0u;
    }

    uint8_t rareCount() const {
        return ((owned_ & 1u) != 0u ? 1u : 0u) +
               ((owned_ & 2u) != 0u ? 1u : 0u) +
               ((owned_ & 4u) != 0u ? 1u : 0u);
    }
    uint8_t playProgress() const { return playProgress_; }
    uint8_t petProgress() const { return petProgress_; }
    uint8_t buildProgress() const { return buildProgress_; }

    // Debug grant and milestone reward share the same unlock operation.
    bool grant(Prize prize) {
        if (prize == Prize::None || owns(prize)) return false;
        owned_ |= mask(prize);
        return true;
    }

    Prize reward(Action action, uint32_t nowMs) {
        if (rareCount() == 3u) return Prize::None;
        bool reached = false;
        switch (action) {
            case Action::Play:
                if (++playProgress_ == 4u) {
                    playProgress_ = 0u;
                    reached = true;
                }
                break;
            case Action::Pet:
                if (++petProgress_ == 5u) {
                    petProgress_ = 0u;
                    reached = true;
                }
                break;
            case Action::BuildSuccess:
                if (++buildProgress_ == 3u) {
                    buildProgress_ = 0u;
                    reached = true;
                }
                break;
        }
        if (!reached ||
            (hasFound_ && static_cast<uint32_t>(nowMs - lastFindMs_) <
                             kCooldownMs)) return Prize::None;

        Prize locked[3] = {Prize::None, Prize::None, Prize::None};
        uint8_t count = 0u;
        const Prize rares[] = {Prize::Crown, Prize::Shades, Prize::StarPin};
        for (const Prize prize : rares) {
            if (!owns(prize)) locked[count++] = prize;
        }
        if (!count) return Prize::None;
        const Prize selected = locked[nextRandom() % count];
        grant(selected);
        lastFindMs_ = nowMs;
        hasFound_ = true;
        return selected;
    }

private:
    static uint8_t mask(Prize prize) {
        switch (prize) {
            case Prize::Crown: return 1u;
            case Prize::Shades: return 2u;
            case Prize::StarPin: return 4u;
            case Prize::None: return 0u;
        }
        return 0u;
    }

    uint32_t nextRandom() {
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        return rng_;
    }

    uint32_t rng_ = 0xC01EC7u;
    uint32_t lastFindMs_ = 0u;
    uint8_t owned_ = 0u;
    uint8_t playProgress_ = 0u;
    uint8_t petProgress_ = 0u;
    uint8_t buildProgress_ = 0u;
    bool hasFound_ = false;
};

} // namespace deskpet_finds
