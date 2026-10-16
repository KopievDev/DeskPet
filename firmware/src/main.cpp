#include <Arduino.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "DeskPetTDisplayS3.h"
#include "FaceAnimator.h"
#include "ReactionController.h"
#include "IdleBehavior.h"
#include "ButtonGesture.h"
#include "PetBond.h"
#include "BuildComedy.h"
#include "UserActivity.h"
#include "PetNeeds.h"
#include "AccessoryWardrobe.h"
#include "PetFinds.h"
#include "PetProgress.h"
#include "PetProgressStorage.h"
#include "ComedyDirector.h"

namespace {
constexpr uint8_t kUserButtonPin = 14;
constexpr uint8_t kBootButtonPin = 0;
constexpr uint32_t kDemoIntervalMs = 1800;
constexpr uint32_t kPetDurationMs = 3200;
constexpr uint32_t kSurpriseDurationMs = 2300;

DeskPetTDisplayS3 display;
FaceAnimator face(display);
ReactionController reactions;
PetBond bond;
deskpet_build_comedy::Story buildStory;
deskpet_activity::Controller userActivity;
deskpet_needs::Controller needs;
deskpet_accessories::Wardrobe wardrobe;
deskpet_finds::Collection finds;

deskpet_accessories::Item accessoryFor(deskpet_finds::Prize prize) {
    switch (prize) {
        case deskpet_finds::Prize::Crown: return deskpet_accessories::Item::Crown;
        case deskpet_finds::Prize::Shades: return deskpet_accessories::Item::Shades;
        case deskpet_finds::Prize::StarPin: return deskpet_accessories::Item::StarPin;
        case deskpet_finds::Prize::None: return deskpet_accessories::Item::None;
    }
    return deskpet_accessories::Item::None;
}

void announceFind(deskpet_finds::Prize prize, uint32_t nowMs) {
    if (prize == deskpet_finds::Prize::None) return;
    Serial.print("FOUND rare accessory: ");
    Serial.println(deskpet_finds::prizeName(prize));
    face.setFindNotice(accessoryFor(prize), nowMs);
}
IdleBehavior ambient;
deskpet_progress::Storage saveStorage;
deskpet_progress::Journal saveJournal;
deskpet_comedy::Director comedy;
bool autoSaveEnabled = true;

deskpet_progress::Snapshot captureProgress() {
    deskpet_progress::Snapshot state;
    state.bond = bond.score();
    state.energy = needs.energy();
    state.mood = needs.mood();
    state.attention = needs.attention();
    const deskpet_finds::Collection::State rare = finds.state();
    state.owned = rare.owned;
    state.play = rare.play;
    state.pet = rare.pet;
    state.builds = rare.builds;
    state.wardrobeMode = wardrobe.mode();
    state.outfit = wardrobe.manualOutfit();
    state.personality = ambient.personality();
    state.needsEnabled = needs.enabled();
    state.bondEnabled = bond.enabled();
    return state;
}

void restoreProgress(const deskpet_progress::Snapshot& state,
                     uint32_t nowMs) {
    bond.restore(state.bond, nowMs);
    bond.setEnabled(state.bondEnabled, nowMs);
    needs.restore(state.energy, state.mood, state.attention,
                  state.needsEnabled, nowMs);
    finds.restore({state.owned, state.play, state.pet, state.builds}, nowMs);
    wardrobe.restore(state.wardrobeMode, state.outfit);
    ambient.setPersonality(state.personality, nowMs);
    face.setPersonality(state.personality, nowMs);
    face.setAffection(bond.score());
    face.setNeeds(needs.energy(), needs.mood(), needs.attention());
}

bool saveProgressNow(uint32_t nowMs) {
    const deskpet_progress::Snapshot snapshot = captureProgress();
    if (!saveStorage.ready() || !saveStorage.write(snapshot)) {
        saveJournal.failed(nowMs);
        return false;
    }
    saveJournal.wrote(snapshot, nowMs);
    return true;
}

void autoSaveProgress(uint32_t nowMs) {
    saveJournal.observe(captureProgress(), nowMs);
    if (autoSaveEnabled && saveStorage.ready() &&
        saveJournal.shouldWrite(nowMs)) {
        if (!saveProgressNow(nowMs))
            Serial.println("NVS save failed; will retry");
    }
}


struct NamedEmotion {
    const char* command;
    Emotion emotion;
};

// Single table for Serial mapping. The enum order also defines USER/demo order.
constexpr NamedEmotion kEmotionCommands[] = {
    {"neutral", Emotion::Neutral},
    {"happy", Emotion::Happy},
    {"curious", Emotion::Curious},
    {"surprised", Emotion::Surprised},
    {"sleepy", Emotion::Sleepy},
    {"angry", Emotion::Angry},
    {"sad", Emotion::Sad},
    {"love", Emotion::Love},
    {"confused", Emotion::Confused},
    {"excited", Emotion::Excited},
    {"playful", Emotion::Playful},
    {"smug", Emotion::Smug},
    {"furious", Emotion::Furious},
    {"embarrassed", Emotion::Embarrassed},
    {"dizzy", Emotion::Dizzy},
    {"bored", Emotion::Bored},
    {"bewildered", Emotion::Bewildered},
    {"nerd", Emotion::Nerd},
    {"derp", Emotion::Derp},
    {"gremlin", Emotion::Gremlin},
    {"panic", Emotion::Panic},
    {"unibrow", Emotion::Unibrow},
    {"scruffy", Emotion::Scruffy},
    {"sideeye", Emotion::SideEye},
    {"puffed", Emotion::Puffed},
    {"goofy", Emotion::Goofy},
};
static_assert(sizeof(kEmotionCommands) / sizeof(kEmotionCommands[0]) ==
                  static_cast<uint8_t>(Emotion::Count),
              "Each emotion needs a Serial command");

deskpet_button::Gesture userButton(true);
deskpet_button::Gesture bootButton(false);
char commandBuffer[48] = {};
size_t commandLength = 0;
bool demoEnabled = false;
uint32_t nextDemoMs = 0;

void cancelAmbient(uint32_t nowMs) {
    ambient.reset(nowMs);
    face.setIdleGesture(IdleGesture::None, nowMs);
}

void syncReaction(uint32_t nowMs) {
    // Do not consume queued UI changes while asleep: apply on wake.
    if (face.isSleeping()) return;
    Emotion next;
    if (reactions.takeChange(next)) {
        cancelAmbient(nowMs);
        face.setNeedPrompt(deskpet_needs::Kind::None);
        face.setEmotion(next, nowMs);
    }
}

void handleUserGesture(deskpet_button::Action action, uint32_t nowMs) {
    if (action == deskpet_button::Action::None) return;
    comedy.activity(nowMs);
    cancelAmbient(nowMs);
    demoEnabled = false;
    face.setNeedPrompt(deskpet_needs::Kind::None);
    if (action == deskpet_button::Action::Hold) {
        bond.pet(nowMs);
        needs.pet();
        announceFind(finds.reward(deskpet_finds::Action::Pet, nowMs), nowMs);
    } else if (action == deskpet_button::Action::DoubleTap) {
        bond.surprise(nowMs);
        needs.surprise();
    } else {
        bond.interact(nowMs);
        needs.interact();
    }
    // A user action is the strongest interaction, ahead of idle cues.

    if (action == deskpet_button::Action::Tap) {
        reactions.nextEmotion(nowMs);
        Serial.println("USER tap -> next emotion");
    } else if (action == deskpet_button::Action::DoubleTap) {
        // Brief surprise, then restore the previous manual mood.
        reactions.showTemporary(Emotion::Surprised, nowMs, kSurpriseDurationMs);
        Serial.println("USER double -> SURPRISED (2.3s)");
    } else if (action == deskpet_button::Action::Hold) {
        // Love eyes + tiny hearts are the visual petting response.
        reactions.showTemporary(Emotion::Love, nowMs, kPetDurationMs);
        Serial.println("USER hold -> PET / LOVE (3.2s)");
    }

    if (face.isSleeping()) face.setSleeping(false, nowMs);
    syncReaction(nowMs);
}

void handleBootGesture(deskpet_button::Action action, uint32_t nowMs) {
    if (action == deskpet_button::Action::None) return;
    comedy.activity(nowMs);
    cancelAmbient(nowMs);
    demoEnabled = false;
    face.setNeedPrompt(deskpet_needs::Kind::None);
    reactions.activity(nowMs);
    if (action == deskpet_button::Action::Hold || face.isSleeping()) {
        bond.interact(nowMs);
        needs.interact();
    }

    if (action == deskpet_button::Action::Tap) {
        if (face.isSleeping()) {
            face.setSleeping(false, nowMs);
            syncReaction(nowMs);
            Serial.println("BOOT tap -> wake");
        } else {
            syncReaction(nowMs); // restore baseline before going to sleep
            face.setSleeping(true, nowMs);
            Serial.println("BOOT tap -> sleep");
        }
    } else if (action == deskpet_button::Action::Hold) {
        // Cycle personality without changing sleep state or enabling sass.
        const auto current = ambient.personality();
        const Personality next = current == Personality::Calm
            ? Personality::Playful
            : current == Personality::Playful
                ? Personality::Cheeky : Personality::Calm;
        syncReaction(nowMs);
        ambient.setPersonality(next, nowMs);
        face.setPersonality(next, nowMs);
        comedy.setPersonality(next, nowMs);
        Serial.print("BOOT hold -> personality ");
        Serial.println(personalityName(next));
    }
}

void showStatus() {
    Serial.print("Mood: ");
    Serial.print(emotionName(reactions.desiredEmotion()));
    Serial.print(" | manual: ");
    Serial.print(emotionName(reactions.manualEmotion()));
    Serial.print(" | idle: ");
    Serial.print(reactions.idleEnabled() ? "on (" : "off (");
    Serial.print(reactions.idleSeconds());
    Serial.print("s) | demo: ");
    Serial.print(demoEnabled ? "on" : "off");
    Serial.print(" | bond: ");
    Serial.print(bond.score());
    Serial.print("/100 (");
    Serial.print(bond.enabled() ? "on" : "off");
    Serial.print(")");
    Serial.print(" | computer: ");
    Serial.print(deskpet_activity::presenceName(userActivity.presence()));
    Serial.print(userActivity.enabled() ? "" : " (off)");
    Serial.print(" | needs: E");
    Serial.print(needs.energy());
    Serial.print(" M");
    Serial.print(needs.mood());
    Serial.print(" T");
    Serial.print(needs.attention());
    Serial.print(needs.enabled() ? "" : " (off)");
    Serial.print(" | save: ");
    Serial.print(saveStorage.ready() ? "ready" : "unavailable");
    Serial.print(autoSaveEnabled ? "/auto" : "/manual");
    Serial.print(saveJournal.dirty() ? "/dirty" : "");
    Serial.print(" | comedy: ");
    Serial.print(comedy.enabled() ? "on/" : "off/");
    Serial.print(comedy.count());
    Serial.print(" | wardrobe: ");
    Serial.print(deskpet_accessories::modeName(wardrobe.mode()));
    Serial.print(" | build failures: ");
    Serial.print(buildStory.consecutiveFailures());
    Serial.print(" | sass: ");
    Serial.print(face.sassEnabled() ? "on" : "off");
    Serial.print(" | personality: ");
    Serial.print(personalityName(ambient.personality()));
    Serial.print(" | ambient: ");
    Serial.print(ambient.enabled() ? "on" : "off");
    Serial.print(" | gesture: ");
    Serial.print(static_cast<int>(ambient.gesture()));
    Serial.print(" | sleeping: ");
    Serial.print(face.isSleeping() ? "yes" : "no");
    Serial.print(" | animation: ");
    Serial.println(face.lifecycleName());
}

void handleCommand(const char* command, uint32_t nowMs) {
    if (strcmp(command, "help") == 0) {
        Serial.println("Emotions: neutral happy curious surprised sleepy angry sad love confused excited playful smug furious embarrassed dizzy bored bewildered nerd derp gremlin panic unibrow scruffy sideeye puffed goofy");
        Serial.println("Comic preview: comic next (Nerd, Derp, Gremlin, Panic, Unibrow, Scruffy, SideEye, Puffed, Goofy)");
        Serial.println("Idle comedy: comedy status | comedy on/off | comedy now");
        Serial.println("Controls: next sleep wake demo on demo off sass on sass off quip ambient on ambient off");
        Serial.println("Buttons: USER tap next; double-tap surprise; hold pet LOVE");
        Serial.println("Buttons: BOOT tap sleep/wake; hold cycle calm/playful/cheeky");
        Serial.println("Personality: personality calm | personality playful | personality cheeky");
        Serial.println("Reactions: event build-success | event build-failure | idle on/off | idle <15..3600> | status");
        Serial.println("Bond: bond on | bond off | bond reset | bond status (RAM only)");
        Serial.println("Build comedy: build status | build reset | event build-success/failure");
        Serial.println("Mac activity: event user-idle | event user-active | activity on/off/status");
        Serial.println("Needs: needs status/reset/on/off | needs coffee/play | needs set E M T");
        Serial.println("Needs debug: needs show coffee/attention/cheer");
        Serial.println("Inventory: inventory | wear status | wear auto/on/off/clear/random");
        Serial.println("Wear: wear beanie/partyhat/glasses/mustache/bowtie/crown/shades/starpin");
        Serial.println("Finds: find status | find reset | find grant crown/shades/starpin");
        Serial.println("Save: save status | save on/off | save now | save reset confirm");
        return;
    }
    if (strcmp(command, "ping") == 0) {
        Serial.println("pong");
        return;
    }
    if (strcmp(command, "status") == 0) {
        showStatus();
        return;
    }
    if (strcmp(command, "save status") == 0) {
        saveJournal.observe(captureProgress(), nowMs);
        Serial.print("NVS: ");
        Serial.print(saveStorage.ready() ? "READY" : "UNAVAILABLE");
        Serial.print(" | autosave: ");
        Serial.print(autoSaveEnabled ? "ON" : "OFF");
        Serial.print(" | stored: ");
        Serial.print(saveJournal.loaded() ? "YES" : "NO");
        Serial.print(" | pending: ");
        Serial.print(saveJournal.dirty() ? "YES" : "NO");
        Serial.print(" | format: v");
        Serial.println(deskpet_progress::kVersion);
        return;
    }
    if (strcmp(command, "save on") == 0 ||
        strcmp(command, "save off") == 0) {
        autoSaveEnabled = strcmp(command, "save on") == 0;
        Serial.println(autoSaveEnabled ? "Autosave ON" : "Autosave OFF");
        return;
    }
    if (strcmp(command, "save now") == 0) {
        saveJournal.observe(captureProgress(), nowMs);
        if (!saveJournal.dirty() && saveJournal.loaded()) {
            Serial.println("Progress already saved");
            return;
        }
        Serial.println(saveProgressNow(nowMs) ?
                       "Progress saved to NVS" : "NVS save failed");
        return;
    }
    if (strcmp(command, "save reset confirm") == 0) {
        if (!saveStorage.clear()) {
            Serial.println("NVS erase FAILED: progress not reset");
            return;
        }
        // Erase both checkpoints AND reset the in-memory game state,
        // otherwise the next autosave would recreate the old progress.
        restoreProgress(deskpet_progress::Snapshot(), nowMs);
        face.setFindNotice(deskpet_accessories::Item::None, nowMs);
        saveJournal.begin(captureProgress(), nowMs, false);
        Serial.println("NVS and pet progress reset to defaults");
        return;
    }
    if (strcmp(command, "save reset") == 0) {
        Serial.println("To erase progress, type: save reset confirm");
        return;
    }
    if (strcmp(command, "comedy status") == 0) {
        Serial.print("Idle comedy: ");
        Serial.print(comedy.enabled() ? "ON" : "OFF");
        Serial.print(" | cameos: ");
        Serial.print(comedy.count());
        Serial.print(" | last: ");
        Serial.print(comedy.lastEmotion() == Emotion::Count
            ? "none" : emotionName(comedy.lastEmotion()));
        Serial.print(" | eligible quiet seconds remaining: ");
        Serial.println(comedy.remainingMs() / 1000u);
        return;
    }
    if (strcmp(command, "comedy on") == 0 ||
        strcmp(command, "comedy off") == 0) {
        const bool on = strcmp(command, "comedy on") == 0;
        comedy.setEnabled(on, nowMs);
        Serial.println(on ? "Idle comedy ON" : "Idle comedy OFF");
        return;
    }
    if (strcmp(command, "comedy now") == 0) {
        // Even manual preview cannot displace a higher-priority reaction.
        const bool canPreview = !face.isSleeping() && !demoEnabled &&
            !reactions.reactionActive() && !reactions.idleActive() &&
            reactions.manualEmotion() == Emotion::Neutral &&
            reactions.desiredEmotion() == Emotion::Neutral &&
            userActivity.presence() != deskpet_activity::Presence::Away;
        if (!canPreview) {
            Serial.println("Comedy blocked: wake up and select neutral first");
            return;
        }
        deskpet_comedy::Cue cue = {Emotion::Neutral, 0u};
        comedy.preview(nowMs, ambient.personality(),
                       needs.energy(), needs.mood(), needs.attention(),
                       bond.score(), cue);
        cancelAmbient(nowMs);
        reactions.showTemporary(cue.emotion, nowMs, cue.durationMs);
        syncReaction(nowMs);
        Serial.print("Comedy preview: ");
        Serial.println(emotionName(cue.emotion));
        return;
    }
    if (strcmp(command, "comic next") == 0) {
        // Rapid preview of the nine acting scenes without running
        // through all 26 moods in USER/demo order.
        comedy.activity(nowMs);
        constexpr Emotion comicMoods[] = {
            Emotion::Nerd, Emotion::Derp, Emotion::Gremlin,
            Emotion::Panic, Emotion::Unibrow, Emotion::Scruffy,
            Emotion::SideEye, Emotion::Puffed, Emotion::Goofy
        };
        Emotion next = comicMoods[0];
        for (uint8_t i = 0; i < sizeof(comicMoods) / sizeof(comicMoods[0]); ++i) {
            if (reactions.manualEmotion() == comicMoods[i]) {
                next = comicMoods[(i + 1u) % (sizeof(comicMoods) / sizeof(comicMoods[0]))];
                break;
            }
        }
        cancelAmbient(nowMs);
        demoEnabled = false;
        reactions.chooseEmotion(next, nowMs);
        bond.interact(nowMs);
        if (face.isSleeping()) face.setSleeping(false, nowMs);
        syncReaction(nowMs);
        Serial.print("Comic: ");
        Serial.println(emotionName(next));
        return;
    }
    if (strcmp(command, "bond status") == 0) {
        Serial.print("Affection: ");
        Serial.print(bond.score());
        Serial.print("/100, comic cameos ");
        Serial.println(bond.enabled() ? "ON" : "OFF");
        return;
    }
    if (strcmp(command, "bond on") == 0 ||
        strcmp(command, "bond off") == 0) {
        bond.setEnabled(strcmp(command, "bond on") == 0, nowMs);
        Serial.println(bond.enabled() ? "Bond cameos ON" : "Bond cameos OFF");
        return;
    }
    if (strcmp(command, "bond reset") == 0) {
        bond.reset(nowMs);
        face.setAffection(bond.score());
        Serial.println("Affection reset to 50/100");
        return;
    }
    if (strcmp(command, "activity status") == 0) {
        Serial.print("Computer activity: ");
        Serial.print(deskpet_activity::presenceName(userActivity.presence()));
        Serial.print(" | monitoring: ");
        Serial.print(userActivity.enabled() ? "ON" : "OFF");
        Serial.print(" | idle jokes: ");
        Serial.println(userActivity.jokeCount());
        return;
    }
    if (strcmp(command, "activity on") == 0 ||
        strcmp(command, "activity off") == 0) {
        const bool enabled = strcmp(command, "activity on") == 0;
        userActivity.setEnabled(enabled, nowMs);
        Serial.println(enabled ? "Host activity jokes ON" :
                                 "Host activity jokes OFF");
        return;
    }
    if (strcmp(command, "event user-idle") == 0 ||
        strcmp(command, "event user-active") == 0) {
        const deskpet_activity::Presence previous = userActivity.presence();
        if (strcmp(command, "event user-idle") == 0) {
            userActivity.idle(nowMs);
        } else {
            userActivity.active(nowMs);
            // An actual return from the Mac can clear a stale internal
            // Bored state, but may not cancel a temporary build reaction.
            if (reactions.idleActive()) {
                reactions.activity(nowMs);
                syncReaction(nowMs);
            }
        }
        if (previous != userActivity.presence()) comedy.activity(nowMs);
        Serial.print("Computer: ");
        Serial.println(deskpet_activity::presenceName(userActivity.presence()));
        return;
    }
    if (strcmp(command, "inventory") == 0) {
        Serial.println("Starter: beanie partyhat glasses mustache bowtie");
        Serial.print("crown: ");
        Serial.println(finds.owns(deskpet_finds::Prize::Crown) ? "OWNED" : "LOCKED");
        Serial.print("shades: ");
        Serial.println(finds.owns(deskpet_finds::Prize::Shades) ? "OWNED" : "LOCKED");
        Serial.print("starpin: ");
        Serial.println(finds.owns(deskpet_finds::Prize::StarPin) ? "OWNED" : "LOCKED");
        Serial.println("Slots: head | eyes | face. All items RAM only.");
        return;
    }
    if (strcmp(command, "find status") == 0) {
        Serial.print("Inventory: ");
        Serial.print(5u + finds.rareCount());
        Serial.print("/8 | build: ");
        Serial.print(finds.buildProgress());
        Serial.print("/3 | play: ");
        Serial.print(finds.playProgress());
        Serial.print("/4 | pet: ");
        Serial.print(finds.petProgress());
        Serial.println("/5 | rare drop cooldown 60s");
        return;
    }
    if (strcmp(command, "find reset") == 0) {
        finds.reset();
        // Rare clothes from the previous game cannot stay equipped.
        wardrobe.clear();
        face.setFindNotice(deskpet_accessories::Item::None, nowMs);
        Serial.println("Finds reset; 5 common items still owned");
        return;
    }
    if (strncmp(command, "find grant ", 11) == 0) {
        const char* name = command + 11;
        deskpet_finds::Prize prize = deskpet_finds::Prize::None;
        if (strcmp(name, "crown") == 0)
            prize = deskpet_finds::Prize::Crown;
        else if (strcmp(name, "shades") == 0)
            prize = deskpet_finds::Prize::Shades;
        else if (strcmp(name, "starpin") == 0)
            prize = deskpet_finds::Prize::StarPin;
        if (prize == deskpet_finds::Prize::None) {
            Serial.println("Use: find grant crown | shades | starpin");
            return;
        }
        if (finds.grant(prize)) announceFind(prize, nowMs);
        else Serial.println("Already owned");
        return;
    }
    if (strcmp(command, "wear status") == 0) {
        const auto outfit = wardrobe.outfit();
        Serial.print("Wardrobe: ");
        Serial.print(deskpet_accessories::modeName(wardrobe.mode()));
        Serial.print(" | head: ");
        Serial.print(deskpet_accessories::itemName(outfit.head));
        Serial.print(" | eyes: ");
        Serial.print(deskpet_accessories::itemName(outfit.eyes));
        Serial.print(" | face: ");
        Serial.println(deskpet_accessories::itemName(outfit.face));
        return;
    }
    if (strcmp(command, "wear auto") == 0 ||
        strcmp(command, "wear off") == 0 ||
        strcmp(command, "wear on") == 0) {
        const auto mode = strcmp(command, "wear auto") == 0
            ? deskpet_accessories::Mode::Auto
            : strcmp(command, "wear off") == 0
                ? deskpet_accessories::Mode::Off
                : deskpet_accessories::Mode::Manual;
        wardrobe.setMode(mode);
        Serial.print("Wardrobe mode: ");
        Serial.println(deskpet_accessories::modeName(mode));
        return;
    }
    if (strcmp(command, "wear clear") == 0 ||
        strcmp(command, "wear none") == 0) {
        wardrobe.clear();
        Serial.println("Outfit cleared");
        return;
    }
    if (strcmp(command, "wear random") == 0) {
        wardrobe.surprise(
            finds.owns(deskpet_finds::Prize::Crown),
            finds.owns(deskpet_finds::Prize::Shades),
            finds.owns(deskpet_finds::Prize::StarPin));
        Serial.println("Surprise outfit equipped");
        return;
    }
    if (strncmp(command, "wear ", 5) == 0) {
        const char* name = command + 5;
        deskpet_accessories::Item item = deskpet_accessories::Item::None;
        if (strcmp(name, "beanie") == 0)
            item = deskpet_accessories::Item::Beanie;
        else if (strcmp(name, "partyhat") == 0)
            item = deskpet_accessories::Item::PartyHat;
        else if (strcmp(name, "glasses") == 0)
            item = deskpet_accessories::Item::Glasses;
        else if (strcmp(name, "mustache") == 0)
            item = deskpet_accessories::Item::Mustache;
        else if (strcmp(name, "bowtie") == 0)
            item = deskpet_accessories::Item::BowTie;
        else if (strcmp(name, "crown") == 0)
            item = deskpet_accessories::Item::Crown;
        else if (strcmp(name, "shades") == 0)
            item = deskpet_accessories::Item::Shades;
        else if (strcmp(name, "starpin") == 0)
            item = deskpet_accessories::Item::StarPin;
        else {
            Serial.println("Use: wear beanie | partyhat | glasses | mustache | bowtie | crown | shades | starpin");
            return;
        }
        if ((item == deskpet_accessories::Item::Crown &&
             !finds.owns(deskpet_finds::Prize::Crown)) ||
            (item == deskpet_accessories::Item::Shades &&
             !finds.owns(deskpet_finds::Prize::Shades)) ||
            (item == deskpet_accessories::Item::StarPin &&
             !finds.owns(deskpet_finds::Prize::StarPin))) {
            Serial.println("LOCKED: earn rare gear by builds, play or USER holds");
            return;
        }
        wardrobe.equip(item);
        Serial.print("Equipped: ");
        Serial.println(deskpet_accessories::itemName(item));
        return;
    }
    if (strcmp(command, "needs status") == 0) {
        Serial.print("Energy: ");
        Serial.print(needs.energy());
        Serial.print(" | mood: ");
        Serial.print(needs.mood());
        Serial.print(" | attention: ");
        Serial.print(needs.attention());
        Serial.print(" | needs: ");
        Serial.println(needs.enabled() ? "ON" : "OFF");
        return;
    }
    if (strcmp(command, "needs reset") == 0) {
        needs.reset(nowMs);
        face.setNeedPrompt(deskpet_needs::Kind::None);
        Serial.println("Needs reset: E78 M70 T65");
        return;
    }
    if (strcmp(command, "needs on") == 0 ||
        strcmp(command, "needs off") == 0) {
        const bool enabled = strcmp(command, "needs on") == 0;
        needs.setEnabled(enabled, nowMs);
        face.setNeedPrompt(deskpet_needs::Kind::None);
        Serial.println(enabled ? "Needs ON" : "Needs OFF");
        return;
    }
    if (strncmp(command, "needs set ", 10) == 0) {
        unsigned int energy, mood, attention;
        char trailing;
        if (sscanf(command + 10, "%u %u %u %c",
                   &energy, &mood, &attention, &trailing) != 3 ||
            energy > 100u || mood > 100u || attention > 100u) {
            Serial.println("Use: needs set <0..100> <0..100> <0..100>");
            return;
        }
        needs.set(static_cast<uint8_t>(energy), static_cast<uint8_t>(mood),
                  static_cast<uint8_t>(attention), nowMs);
        Serial.println("Needs updated");
        return;
    }
    if (strcmp(command, "needs coffee") == 0 ||
        strcmp(command, "needs play") == 0 ||
        strncmp(command, "needs show ", 11) == 0) {
        const bool coffee = strcmp(command, "needs coffee") == 0;
        const bool play = strcmp(command, "needs play") == 0;
        const char* preview = strncmp(command, "needs show ", 11) == 0
            ? command + 11 : "";
        deskpet_needs::Cue cue = {Emotion::Neutral,
                                  deskpet_needs::Kind::None, 4800u};
        if (coffee || (strcmp(preview, "coffee") == 0 && !play)) {
            cue = {Emotion::Nerd, deskpet_needs::Kind::Coffee, 4800u};
        } else if (play || strcmp(preview, "attention") == 0) {
            cue = {Emotion::Derp, deskpet_needs::Kind::Attention, 4800u};
        } else if (strcmp(preview, "cheer") == 0) {
            cue = {Emotion::Excited, deskpet_needs::Kind::Cheer, 4800u};
        } else {
            Serial.println("Use: needs show coffee | attention | cheer");
            return;
        }
        if (coffee) needs.coffee();
        else if (play) {
            needs.play();
            announceFind(finds.reward(deskpet_finds::Action::Play, nowMs), nowMs);
        }
        if (face.isSleeping()) face.setSleeping(false, nowMs);
        comedy.activity(nowMs);
        cancelAmbient(nowMs);
        demoEnabled = false;
        reactions.showTemporary(cue.emotion, nowMs, cue.durationMs);
        syncReaction(nowMs);
        face.setNeedPrompt(cue.kind);
        Serial.print("Needs: ");
        Serial.println(coffee ? "coffee served" : play ? "played" : "preview");
        return;
    }
    if (strcmp(command, "build status") == 0) {
        Serial.print("Consecutive build failures: ");
        Serial.println(buildStory.consecutiveFailures());
        return;
    }
    if (strcmp(command, "build reset") == 0) {
        buildStory.reset();
        Serial.println("Build failure history reset");
        return;
    }
    if (strcmp(command, "ambient on") == 0 ||
        strcmp(command, "ambient off") == 0) {
        ambient.setEnabled(strcmp(command, "ambient on") == 0, nowMs);
        face.setIdleGesture(IdleGesture::None, nowMs);
        Serial.println(ambient.enabled() ? "Ambient gestures ON" :
                                            "Ambient gestures OFF");
        return;
    }
    if (strncmp(command, "personality ", 12) == 0) {
        const char* name = command + 12;
        Personality next = Personality::Calm;
        if (strcmp(name, "calm") == 0) next = Personality::Calm;
        else if (strcmp(name, "playful") == 0) next = Personality::Playful;
        else if (strcmp(name, "cheeky") == 0) next = Personality::Cheeky;
        else {
            Serial.println("Choose: personality calm | personality playful | personality cheeky");
            return;
        }
        cancelAmbient(nowMs);
        reactions.activity(nowMs);
        bond.interact(nowMs);
        syncReaction(nowMs);
        ambient.setPersonality(next, nowMs);
        face.setPersonality(next, nowMs);
        comedy.setPersonality(next, nowMs);
        Serial.print("Personality: ");
        Serial.println(personalityName(next));
        return;
    }

    if (strcmp(command, "sass on") == 0 || strcmp(command, "sass off") == 0) {
        cancelAmbient(nowMs);
        reactions.activity(nowMs);
        syncReaction(nowMs);
        const bool enabled = strcmp(command, "sass on") == 0;
        face.setSassEnabled(enabled, nowMs);
        Serial.println(enabled ? "Sass ON (RAM only)" : "Sass OFF");
        return;
    }
    if (strcmp(command, "quip") == 0) {
        cancelAmbient(nowMs);
        Serial.println(face.showQuip(nowMs) ? "Comic quip!" : "Enable it with sass on");
        return;
    }

    if (strcmp(command, "demo on") == 0) {
        comedy.activity(nowMs);
        cancelAmbient(nowMs);
        demoEnabled = true;
        reactions.chooseEmotion(Emotion::Neutral, nowMs);
        if (face.isSleeping()) face.setSleeping(false, nowMs);
        syncReaction(nowMs);
        nextDemoMs = nowMs + kDemoIntervalMs;
        Serial.println("Demo ON: cycling all 26 emotions");
        return;
    }
    if (strcmp(command, "demo off") == 0) {
        comedy.activity(nowMs);
        cancelAmbient(nowMs);
        demoEnabled = false;
        reactions.activity(nowMs);
        syncReaction(nowMs);
        Serial.println("Demo OFF");
        return;
    }

    if (strcmp(command, "idle on") == 0 || strcmp(command, "idle off") == 0) {
        cancelAmbient(nowMs);
        reactions.setIdleEnabled(strcmp(command, "idle on") == 0, nowMs);
        syncReaction(nowMs);
        Serial.println(reactions.idleEnabled() ? "Idle reactions ON" : "Idle reactions OFF");
        return;
    }
    if (strncmp(command, "idle ", 5) == 0) {
        char* end = nullptr;
        const unsigned long seconds = strtoul(command + 5, &end, 10);
        if (end == command + 5 || *end != '\0' ||
            seconds < ReactionController::kMinIdleSeconds ||
            seconds > ReactionController::kMaxIdleSeconds) {
            Serial.println("Idle timeout must be 15..3600 seconds");
            return;
        }
        cancelAmbient(nowMs);
        reactions.setIdleSeconds(static_cast<uint32_t>(seconds), nowMs);
        syncReaction(nowMs);
        Serial.print("Idle timeout set to ");
        Serial.print(seconds);
        Serial.println(" seconds");
        return;
    }

    if (strcmp(command, "sleep") == 0 || strcmp(command, "wake") == 0) {
        comedy.activity(nowMs);
        cancelAmbient(nowMs);
        demoEnabled = false;
        reactions.activity(nowMs);
        if (strcmp(command, "sleep") == 0) {
            syncReaction(nowMs); // restore manual baseline before sleeping
            face.setSleeping(true, nowMs);
        } else {
            face.setSleeping(false, nowMs);
            syncReaction(nowMs);
        }
        Serial.println(face.isSleeping() ? "Sleeping" : "Awake");
        return;
    }

    if (strcmp(command, "event build-success") == 0 ||
        strcmp(command, "event build-failure") == 0) {
        if (face.isSleeping()) {
            Serial.println("Event ignored while DeskPet is sleeping");
            return;
        }
        comedy.activity(nowMs);
        cancelAmbient(nowMs);
        demoEnabled = false;
        const bool success = strcmp(command, "event build-success") == 0;
        bond.build(success, nowMs);
        needs.build(success);
        if (success)
            announceFind(finds.reward(deskpet_finds::Action::BuildSuccess,
                                      nowMs), nowMs);
        const deskpet_build_comedy::Cue cue =
            buildStory.record(success, nowMs);
        // Keep the manually chosen baseline. A button press or a direct
        // Serial mood command can interrupt this little joke at any time.
        reactions.showTemporary(cue.emotion, nowMs, cue.durationMs);
        syncReaction(nowMs);
        Serial.print(success ? "Build OK -> " : "Build failed -> ");
        Serial.print(emotionName(cue.emotion));
        Serial.print(" (");
        Serial.print(cue.durationMs / 1000u);
        Serial.print(".");
        Serial.print((cue.durationMs % 1000u) / 100u);
        Serial.print("s, failure streak ");
        Serial.print(buildStory.consecutiveFailures());
        Serial.println(")");
        return;
    }

    if (strcmp(command, "next") == 0) {
        comedy.activity(nowMs);
        cancelAmbient(nowMs);
        demoEnabled = false;
        reactions.nextEmotion(nowMs);
        bond.interact(nowMs);
        if (face.isSleeping()) face.setSleeping(false, nowMs);
        syncReaction(nowMs);
        Serial.print("Mood: ");
        Serial.println(emotionName(reactions.manualEmotion()));
        return;
    }
    for (const auto& item : kEmotionCommands) {
        if (strcmp(command, item.command) == 0) {
            comedy.activity(nowMs);
            cancelAmbient(nowMs);
            demoEnabled = false;
            reactions.chooseEmotion(item.emotion, nowMs);
            bond.interact(nowMs);
            if (face.isSleeping()) face.setSleeping(false, nowMs);
            syncReaction(nowMs);
            Serial.print("Mood: ");
            Serial.println(emotionName(reactions.manualEmotion()));
            return;
        }
    }
    if (*command) {
        Serial.println("Unknown command. Type help.");
    }
}

void readSerial(uint32_t nowMs) {
    while (Serial.available() > 0) {
        const char value = static_cast<char>(Serial.read());
        if (value == '\r') continue;
        if (value == '\n') {
            commandBuffer[commandLength] = '\0';
            handleCommand(commandBuffer, nowMs);
            commandLength = 0;
        } else if (commandLength < sizeof(commandBuffer) - 1) {
            commandBuffer[commandLength++] = value;
        } else {
            commandLength = 0;
            Serial.println("Command too long.");
        }
    }
}
} // namespace

void setup() {
    Serial.begin(115200);
    pinMode(kUserButtonPin, INPUT_PULLUP);
    pinMode(kBootButtonPin, INPUT_PULLUP);
    const uint32_t buttonsStartedAt = millis();
    userButton.begin(digitalRead(kUserButtonPin) == LOW, buttonsStartedAt);
    bootButton.begin(digitalRead(kBootButtonPin) == LOW, buttonsStartedAt);

    if (!display.begin(1, 200)) {
        Serial.println("LCD initialization failed");
        return;
    }
    if (!face.begin()) {
        Serial.println("Could not allocate face sprite (check PSRAM)");
        return;
    }
    const uint32_t startedAt = millis();
    reactions.begin(startedAt);
    buildStory.begin();
    userActivity.begin(startedAt);
    needs.begin(startedAt);
    wardrobe.begin(static_cast<uint32_t>(micros()) ^ 0x5C8330A1u);
    finds.begin(static_cast<uint32_t>(micros()) ^ 0xD35CA77u);
    bond.begin(startedAt, static_cast<uint32_t>(micros()) ^ 0x682C95B3u);
    face.setAffection(bond.score());
    face.setNeeds(needs.energy(), needs.mood(), needs.attention());
    ambient.begin(startedAt, static_cast<uint32_t>(micros()) ^ 0xB16B00B5u);
    const bool nvsReady = saveStorage.begin();
    deskpet_progress::Snapshot restored;
    const bool loaded = nvsReady && saveStorage.load(restored);
    if (loaded) restoreProgress(restored, startedAt);
    comedy.begin(startedAt,
                 static_cast<uint32_t>(micros()) ^ 0xC09ED1A1u,
                 ambient.personality());
    saveJournal.begin(captureProgress(), startedAt, loaded);
    Serial.println(nvsReady
        ? (loaded ? "Pet progress restored from NVS" :
                    "No valid NVS save; using defaults")
        : "NVS unavailable; pet progress remains in RAM");
    Serial.println("DeskPet 0.1 ready. Type help.");
}

void loop() {
    const uint32_t now = millis();
    handleUserGesture(
        userButton.poll(digitalRead(kUserButtonPin) == LOW, now), now);
    handleBootGesture(
        bootButton.poll(digitalRead(kBootButtonPin) == LOW, now), now);

    readSerial(now);
    needs.advance(now, face.isSleeping());
    if (demoEnabled && static_cast<int32_t>(now - nextDemoMs) >= 0) {
        reactions.nextEmotion(now);
        syncReaction(now);
        nextDemoMs = now + kDemoIntervalMs;
    }
    if (!demoEnabled && !face.isSleeping()) {
        reactions.tick(now);
        syncReaction(now);
    }
    const bool allowAmbient = !demoEnabled && !face.isSleeping() &&
        !reactions.reactionActive() && !reactions.idleActive() &&
        reactions.desiredEmotion() == Emotion::Neutral &&
        reactions.manualEmotion() == Emotion::Neutral;

    // Host-driven comedy has precedence over random PetBond cameos.
    // If the Mac has reported AWAY, PetBond cannot randomly interrupt
    // a carefully-timed activity skit.
    const bool allowActivity = allowAmbient;
    deskpet_activity::Cue activityCue = {Emotion::Neutral, 0u};
    if (userActivity.tick(now, allowActivity, ambient.personality(),
                          bond.score(), activityCue)) {
        cancelAmbient(now);
        reactions.showTemporary(activityCue.emotion, now,
                                activityCue.durationMs);
        syncReaction(now);
    }

    // Needs are strictly lower priority than host presence and builds.
    // Never interrupt a manual mood, sleep, demo, or a running skit.
    deskpet_needs::Cue needsCue = {
        Emotion::Neutral, deskpet_needs::Kind::None, 0u
    };
    const bool allowNeeds = allowAmbient &&
        !reactions.reactionActive() &&
        userActivity.presence() != deskpet_activity::Presence::Away;
    if (needs.tick(now, allowNeeds, ambient.personality(), needsCue)) {
        cancelAmbient(now);
        reactions.showTemporary(needsCue.emotion, now, needsCue.durationMs);
        syncReaction(now);
        face.setNeedPrompt(needsCue.kind);
    }

    Emotion comic = Emotion::Neutral;
    const bool allowBond = allowNeeds &&
        !reactions.reactionActive();
    if (bond.tick(now, !face.isSleeping(), allowBond,
                  ambient.personality(), comic)) {
        cancelAmbient(now);
        reactions.showTemporary(comic, now, PetBond::kComicDurationMs);
        syncReaction(now);
    }
    // Below Mac presence, Needs and Bond, above cosmetic idle gestures.
    // Accumulate eligible time; never queue events while a reaction is active.
    deskpet_comedy::Cue idleComedy = {Emotion::Neutral, 0u};
    const bool allowComedy = allowBond && !reactions.reactionActive() &&
        !ambient.isActive() &&
        userActivity.presence() != deskpet_activity::Presence::Away;
    if (comedy.tick(now, allowComedy, ambient.personality(),
                    needs.energy(), needs.mood(), needs.attention(),
                    bond.score(), idleComedy)) {
        cancelAmbient(now);
        reactions.showTemporary(idleComedy.emotion, now,
                                idleComedy.durationMs);
        syncReaction(now);
    }
    face.setAffection(bond.score());
    face.setNeeds(needs.energy(), needs.mood(), needs.attention());
    wardrobe.tick(reactions.desiredEmotion(), ambient.personality(),
                 needs.energy(), needs.mood(), needs.attention(),
                 face.isSleeping());
    deskpet_accessories::Outfit currentOutfit = wardrobe.outfit();
    // Unlocked rares are used by Auto; manual mode never gets overridden.
    if (wardrobe.mode() == deskpet_accessories::Mode::Auto &&
        !face.isSleeping()) {
        const Emotion mood = reactions.desiredEmotion();
        if (finds.owns(deskpet_finds::Prize::Crown) &&
            needs.mood() >= 90u && mood != Emotion::Sleepy)
            currentOutfit.head = deskpet_accessories::Item::Crown;
        if (finds.owns(deskpet_finds::Prize::Shades) &&
            mood == Emotion::Smug)
            currentOutfit.eyes = deskpet_accessories::Item::Shades;
        if (finds.owns(deskpet_finds::Prize::StarPin) &&
            mood == Emotion::Excited)
            currentOutfit.face = deskpet_accessories::Item::StarPin;
    }
    face.setAccessories(currentOutfit);
    if (!reactions.reactionActive()) {
        face.setNeedPrompt(deskpet_needs::Kind::None);
    }
    if (ambient.tick(now, allowAmbient && !reactions.reactionActive() &&
                     userActivity.presence() !=
                         deskpet_activity::Presence::Away)) {
        face.setIdleGesture(ambient.gesture(), ambient.startedMs());
    }

    autoSaveProgress(now);
    face.update(now);
    delay(1);
}
