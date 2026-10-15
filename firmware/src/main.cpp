#include <Arduino.h>
#include "DeskPetTDisplayS3.h"

#include "FaceAnimator.h"

namespace {
constexpr uint8_t kUserButtonPin = 14;
constexpr uint8_t kBootButtonPin = 0;
constexpr uint32_t kDebounceMs = 35;

DeskPetTDisplayS3 display;
FaceAnimator face(display);

struct Button {
    explicit Button(uint8_t gpio) : pin(gpio) {}

    void begin() {
        pinMode(pin, INPUT_PULLUP);
        lastRaw = digitalRead(pin);
        stable = lastRaw;
        changedAt = millis();
    }

    // Emits one event for each stable press; never blocks the render loop.
    bool pressed(uint32_t nowMs) {
        const bool raw = digitalRead(pin);
        if (raw != lastRaw) {
            lastRaw = raw;
            changedAt = nowMs;
        }
        if (nowMs - changedAt >= kDebounceMs && stable != raw) {
            stable = raw;
            return !stable; // active-low
        }
        return false;
    }

    uint8_t pin;
    bool lastRaw = HIGH;
    bool stable = HIGH;
    uint32_t changedAt = 0;
};

Button userButton(kUserButtonPin);
Button bootButton(kBootButtonPin);

char commandBuffer[40] = {};
size_t commandLength = 0;

void handleCommand(const char* command, uint32_t nowMs) {
    if (strcmp(command, "help") == 0) {
        Serial.println("Commands: neutral happy curious surprised sleepy angry sad love confused excited playful sleep wake next ping help");
    } else if (strcmp(command, "ping") == 0) {
        Serial.println("pong");
    } else if (strcmp(command, "sleep") == 0) {
        face.setSleeping(true, nowMs);
    } else if (strcmp(command, "wake") == 0) {
        face.setSleeping(false, nowMs);
    } else if (strcmp(command, "next") == 0) {
        face.nextEmotion(nowMs);
    } else if (strcmp(command, "neutral") == 0) {
        face.setEmotion(Emotion::Neutral, nowMs);
    } else if (strcmp(command, "happy") == 0) {
        face.setEmotion(Emotion::Happy, nowMs);
    } else if (strcmp(command, "curious") == 0) {
        face.setEmotion(Emotion::Curious, nowMs);
    } else if (strcmp(command, "surprised") == 0) {
        face.setEmotion(Emotion::Surprised, nowMs);
    } else if (strcmp(command, "sleepy") == 0) {
        face.setEmotion(Emotion::Sleepy, nowMs);
    } else if (strcmp(command, "angry") == 0) {
        face.setEmotion(Emotion::Angry, nowMs);
    } else if (strcmp(command, "sad") == 0) {
        face.setEmotion(Emotion::Sad, nowMs);
    } else if (strcmp(command, "love") == 0) {
        face.setEmotion(Emotion::Love, nowMs);
    } else if (strcmp(command, "confused") == 0) {
        face.setEmotion(Emotion::Confused, nowMs);
    } else if (strcmp(command, "excited") == 0) {
        face.setEmotion(Emotion::Excited, nowMs);
    } else if (strcmp(command, "playful") == 0) {
        face.setEmotion(Emotion::Playful, nowMs);
    } else if (*command) {
        Serial.println("Unknown command. Type help.");
        return;
    }

    if (*command && strcmp(command, "help") != 0 && strcmp(command, "ping") != 0) {
        Serial.print("State: ");
        Serial.println(face.isSleeping() ? "sleeping" : "awake");
    }
}

void readSerial(uint32_t nowMs) {
    // Bounded line buffer: never block waiting for the end of a command.
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
    userButton.begin();
    bootButton.begin();

    if (!display.begin(1, 200)) {
        Serial.println("LCD initialization failed");
        return;
    }
    if (!face.begin()) {
        Serial.println("Could not allocate face sprite (check PSRAM)");
        return;
    }

    Serial.println("DeskPet 0.1 ready. Type help.");
}

void loop() {
    const uint32_t now = millis();
    if (userButton.pressed(now)) face.nextEmotion(now);
    if (bootButton.pressed(now)) face.setSleeping(!face.isSleeping(), now);
    readSerial(now);
    face.update(now);
    delay(1);
}
