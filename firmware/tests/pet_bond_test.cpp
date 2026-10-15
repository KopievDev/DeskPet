#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "PetBond.h"

static Emotion waitForComic(PetBond& pet, uint32_t start, Personality mode) {
    Emotion cameo = Emotion::Neutral;
    assert(!pet.tick(start, true, true, mode, cameo));
    for (uint32_t delta = 1000u; delta <= 87000u; delta += 1000u) {
        if (pet.tick(start + delta, true, true, mode, cameo)) return cameo;
    }
    assert(false && "Affection cameo did not appear");
    return Emotion::Count;
}

int main() {
    PetBond pet;
    pet.begin(1000u, 777u);
    assert(pet.score() == 50u && pet.enabled());

    pet.interact(1100u);
    assert(pet.score() == 51u);
    pet.interact(1300u); // debounced; no reward for button spam
    assert(pet.score() == 51u);
    pet.interact(3000u);
    assert(pet.score() == 52u);
    pet.surprise(4000u);
    assert(pet.score() == 54u);
    pet.pet(5000u);
    assert(pet.score() == 63u);
    pet.build(true, 6000u);
    assert(pet.score() == 65u);
    pet.build(false, 6100u);
    assert(pet.score() == 64u);

    for (int i = 0; i < 20; ++i) pet.pet(7000u + i * 100u);
    assert(pet.score() == 100u); // saturates instead of overflowing
    Emotion high = waitForComic(pet, 10000u, Personality::Playful);
    assert(high == Emotion::Derp || high == Emotion::Nerd);
    assert(pet.score() == 100u); // a cameo doesn't change affection

    pet.reset(100000u);
    assert(pet.score() == 50u);
    Emotion neutral = waitForComic(pet, 100000u, Personality::Calm);
    assert(neutral == Emotion::Nerd);
    assert(!pet.tick(189000u, true, false, Personality::Calm, neutral));
    assert(!pet.tick(189001u, true, true, Personality::Calm, neutral));
    // The scheduler restarts on re-entry; no immediate surprise after sleep.
    assert(!pet.tick(189100u, false, true, Personality::Calm, neutral));
    assert(!pet.tick(189101u, true, true, Personality::Calm, neutral));

    pet.setEnabled(false, 200000u);
    assert(!pet.enabled());
    for (uint32_t t = 200001; t <= 360000; t += 1000u) {
        assert(!pet.tick(t, true, true, Personality::Cheeky, neutral));
    }
    pet.pet(360100u); // affection still grows when cameos are disabled
    assert(pet.score() == 59u);
    pet.setEnabled(true, 360100u);

    pet.reset(400000u);
    // 31 minutes of awake idle decay -> 3 points, not every frame.
    assert(!pet.tick(400000u, true, false, Personality::Calm, neutral));
    assert(!pet.tick(2260000u, true, false, Personality::Calm, neutral));
    assert(pet.score() == 47u);
    assert(!pet.tick(2260100u, true, false, Personality::Calm, neutral));
    assert(pet.score() == 47u);

    // Low affection changes only comic cameo selection; never overrides
    // explicit emotions. Decay at most 100 points in a long awake period.
    pet.reset(2300000u);
    assert(!pet.tick(2300000u + 40000000u, true, false,
                     Personality::Calm, neutral));
    assert(pet.score() == 0u);
    const uint32_t calmStart = 42500000u;
    assert(waitForComic(pet, calmStart, Personality::Calm) == Emotion::Panic);
    assert(waitForComic(pet, calmStart + 100000u,
                        Personality::Cheeky) == Emotion::Gremlin);

    // wrap-around in elapsed and scheduling math
    const uint32_t edge = UINT32_MAX - 5000u;
    pet.begin(edge, 999u);
    pet.pet(edge + 100u);
    assert(pet.score() == 59u);
    assert(!pet.tick(edge + 600001u, true, false, Personality::Calm, neutral));
    assert(pet.score() == 58u);

    puts("PASS: affection cooldown, saturating score, idle decay, rare comics, priorities");
    return 0;
}
