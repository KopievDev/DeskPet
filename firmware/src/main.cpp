#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "DeskPetTDisplayS3.h"
#include "FaceAnimator.h"
#include "ReactionController.h"
#include "IdleBehavior.h"
#include "ButtonGesture.h"

namespace {
constexpr uint8_t kUserButtonPin = 14;
constexpr uint8_t kBootButtonPin = 0;
constexpr uint32_t kDemoIntervalMs = 1800;
constexpr uint32_t kPetDurationMs = 3200;
constexpr uint32_t kSurpriseDurationMs = 2300;

DeskPetTDisplayS3 display;
FaceAnimator face(display);
ReactionController reactions;
IdleBehavior ambient;

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
        face.setEmotion(next, nowMs);
    }
}

void handleUserGesture(deskpet_button::Action action, uint32_t nowMs) {
    if (action == deskpet_button::Action::None) return;
    cancelAmbient(nowMs);
    demoEnabled = false;

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
    cancelAmbient(nowMs);
    demoEnabled = false;
    reactions.activity(nowMs);

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
        Serial.println("Emotions: neutral happy curious surprised sleepy angry sad love confused excited playful smug furious embarrassed dizzy bored bewildered");
        Serial.println("Controls: next sleep wake demo on demo off sass on sass off quip ambient on ambient off");
        Serial.println("Buttons: USER tap next; double-tap surprise; hold pet LOVE");
        Serial.println("Buttons: BOOT tap sleep/wake; hold cycle calm/playful/cheeky");
        Serial.println("Personality: personality calm | personality playful | personality cheeky");
        Serial.println("Reactions: event build-success | event build-failure | idle on | idle off | idle <15..3600 seconds> | status");
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
        syncReaction(nowMs);
        ambient.setPersonality(next, nowMs);
        face.setPersonality(next, nowMs);
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
        cancelAmbient(nowMs);
        demoEnabled = true;
        reactions.chooseEmotion(Emotion::Neutral, nowMs);
        if (face.isSleeping()) face.setSleeping(false, nowMs);
        syncReaction(nowMs);
        nextDemoMs = nowMs + kDemoIntervalMs;
        Serial.println("Demo ON: cycling all 17 emotions");
        return;
    }
    if (strcmp(command, "demo off") == 0) {
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
        cancelAmbient(nowMs);
        demoEnabled = false;
        const bool success = strcmp(command, "event build-success") == 0;
        reactions.buildResult(success, nowMs);
        syncReaction(nowMs);
        Serial.println(success ? "Build OK -> Excited (4.5s)" :
                                 "Build failed -> Furious (4.5s)");
        return;
    }

    if (strcmp(command, "next") == 0) {
        cancelAmbient(nowMs);
        demoEnabled = false;
        reactions.nextEmotion(nowMs);
        if (face.isSleeping()) face.setSleeping(false, nowMs);
        syncReaction(nowMs);
        Serial.print("Mood: ");
        Serial.println(emotionName(reactions.manualEmotion()));
        return;
    }
    for (const auto& item : kEmotionCommands) {
        if (strcmp(command, item.command) == 0) {
            cancelAmbient(nowMs);
            demoEnabled = false;
            reactions.chooseEmotion(item.emotion, nowMs);
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
    ambient.begin(startedAt, static_cast<uint32_t>(micros()) ^ 0xB16B00B5u);
    Serial.println("DeskPet 0.1 ready. Type help.");
}

void loop() {
    const uint32_t now = millis();
    handleUserGesture(
        userButton.poll(digitalRead(kUserButtonPin) == LOW, now), now);
    handleBootGesture(
        bootButton.poll(digitalRead(kBootButtonPin) == LOW, now), now);

    readSerial(now);
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
    if (ambient.tick(now, allowAmbient)) {
        face.setIdleGesture(ambient.gesture(), ambient.startedMs());
    }

    face.update(now);
    delay(1);
}
