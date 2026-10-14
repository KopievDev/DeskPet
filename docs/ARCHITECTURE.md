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
7. **Hardware portability.** Board-specific display implementation is separate from character logic; touch/AMOLED variants get their own profiles.

## Firmware 0.1 boundaries

The current implementation has **no Wi-Fi, audio, AI API, cloud service, OTA or servos**. It only controls the existing screen and buttons, and accepts local USB Serial commands. This makes the first hardware bring-up easy to debug.

## Technology choices

For 0.1: **PlatformIO / Arduino framework / LILYGO LovyanGFX board profile**. This gives a correct ST7789V 8-bit-parallel pin map and LCD power/backlight initialization without maintaining a fork of the official graphics driver.

After validating the hardware, evaluate a migration to **ESP-IDF + LVGL** for UI complexity, networking, audio and OTA. Do not migrate solely for architecture aesthetics.
