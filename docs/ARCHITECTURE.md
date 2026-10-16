## Autonomous idle comedy (pure core + priority boundary)

`firmware/include/ComedyDirector.h` is a C++11 scheduler driven from `main.cpp::loop()`, with no Arduino headers, heap or storage. When Neutral, awake and eligible after Mac activity, Needs and PetBond have had first refusal, it accrues *eligible millis* and optionally emits a 4.8-second transient reaction. The choice depends on personality + care/bond values but does not mutate them. Ambient gestures temporarily pause accumulation. `ReactionController` owns interruption and return to the manually selected Neutral. Explicit USER/BOOT, Serial emotion, new builds or presence transitions reset the quiet timer; `comedy on/off/status/now` control this feature without writing to NVS.

## Persistent progress boundary (NVS)

`PetProgress.h` is C++11-only, with a portable versioned encoder/decoder, CRC32 validation, slot selection and save scheduling. The ESP32-specific `PetProgressStorage.h` includes `Preferences.h`, writes alternating `deskpet/saveA` and `deskpet/saveB` 28-byte blobs, and loads the newest valid generation. `main.cpp` observes controller state without allocating per frame; it writes only on schedule, on explicit `save now`, or clears both records through `save reset confirm`. Runtime timers, sass, manual expressions and sleep remain ephemeral. This separation permits compiler-only tests without physical hardware.

# Architecture (target, not all implemented)

```text
T-Display-S3 / ESP32-S3             Local Mac app / AI Gateway
┌────────────────────────┐ Wi-Fi   ┌─────────────────────────────┐
│ Face, state, animation │◀───────▶│ Secure local pairing        │
│ buttons, future audio  │ events  │ SwiftUI, model adapters      │
│ sensors, motion, OTA   │         │ GitHub / Xcode / automations │
└────────────────────────┘         └──────────────┬──────────────┘
                                                 │ outbound requests
                                       OpenAI / Gemini / Ollama
```

## Design boundaries

1. **Offline-first firmware.** Rendering, idle behaviour, buttons and simple reactions continue without Mac or Internet.
2. **No API secrets on ESP32.** Tokens remain in Keychain on macOS or in a protected Gateway secret store. Gateway mediates AI calls.
3. **User consent.** Pairing must be confirmed physically; microphone and integrations are opt-in; audio capture has a visible state; controls must include a physical mic-off switch when microphone hardware is added.
4. **Limited AI actions.** Model may suggest a named emotion or permitted tool invocation; firmware and app validate the allowed action set. Destructive or externally visible actions need confirmation.
5. **Device is not reachable from public Internet.** No port forwarding. Remote access, if added, goes through an authenticated, TLS-protected gateway.
6. **Versioned events.** Future connection should be resilient to reconnects, out-of-order messages and updated firmware.
7. **Hardware portability.** Start and validate on the existing LILYGO T-Display-S3 IPS LCD (ST7789V); only afterwards implement an AMOLED profile (RM67162). Board-specific display and GPIO configuration must be separate from character logic; touch is optional and revision-specific.

## Firmware 0.1 boundaries

The current implementation has **no Wi-Fi, audio, AI API, cloud service, OTA or servos**. It only controls the existing screen and buttons, and accepts local USB Serial commands. This makes the first hardware bring-up easy to debug.

## Technology choices

For 0.1: **PlatformIO / Arduino framework / LovyanGFX 1.2.33** and the project-local adapter `firmware/include/DeskPetTDisplayS3.h`. It reproduces the official LILYGO ST7789V 8-bit parallel pin map, LCD power-enable and PWM backlight config, but uses `Bus_Parallel8::config_t::port = 0` for the ESP32-S3 LCD_CAM peripheral. The vendor library's `i2s_port` field does not exist in LovyanGFX 1.2.33's ESP32-S3 bus. Build must be verified on the user's hardware; do not modify files inside `.pio`.

After validating the hardware, evaluate a migration to **ESP-IDF + LVGL** for UI complexity, networking, audio and OTA. Do not migrate solely for architecture aesthetics.

## Display migration plan (decision: IPS first, AMOLED second)

**Current supported target:** LILYGO T-Display-S3 IPS LCD, 170×320 (landscape 320×170), ST7789V on 8-bit parallel. Existing firmware's LILYGO LovyanGFX driver remains the initial implementation. Do not switch the active firmware or pinout to AMOLED while validating the IPS prototype.

**Future target:** LILYGO T-Display-S3 AMOLED 1.91-inch, 240×536 (landscape 536×240), RM67162 / QSPI; the display driver, initialization and pin mapping are **not** interchangeable with IPS. A standalone AMOLED panel intended for a custom PCB requires its **own** verified panel/FPC pinout and electrical design.

Suggested separation:

```text
CharacterState / FaceAnimations / Notifications
                     │ logical pixels / normalized layout
             DisplayRenderer
                     │
               DisplayDriver
                 ├── IPS_ST7789V_170x320  [now]
                 └── AMOLED_RM67162_240x536  [later]
                     │
               BoardProfile
                 ├── TDisplayS3_IPS
                 └── TDisplayS3_AMOLED
```

The hardware profile should supply display driver choice, rotation, width/height, brightness/sleep controls, GPIO reservations, I²C/I²S mappings, button/touch capabilities and PSRAM budget. Firmware implementation may simplify the interface until the second target is actually being brought up; avoid prematurely adding dependencies for the AMOLED driver.

**Acceptance:** bring up IPS and verify animations/FPS first; extract the abstraction without regressing IPS; subsequently add AMOLED support on a separate development branch, validate audio/BLE/Wi-Fi coexistence and redo the pinout/PCB before assembly.
