#!/usr/bin/env python3
"""DeskPet source-only regression checks. No PlatformIO or ESP32 required."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "include/Emotion.h").read_text(encoding="utf-8")
face_header = (ROOT / "src/FaceAnimator.h").read_text(encoding="utf-8")
source = (ROOT / "src/FaceAnimator.cpp").read_text(encoding="utf-8")
serial = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
font = (ROOT / "include/ComicFont.h").read_text(encoding="utf-8")

enum_match = re.search(r"enum class Emotion\s*:\s*uint8_t\s*\{(.*?)\}", header, re.S)
assert enum_match, "Emotion enum missing"
emotions = re.findall(r"\b([A-Za-z]+)(?:\s*=\s*0)?\s*,", enum_match.group(1))
emotions = [name for name in emotions if name != "Count"]
assert len(emotions) == 17, f"Expected 17 emotions; got {len(emotions)}"

for emotion in emotions:
    assert f"case Emotion::{emotion}: return" in source, f"Name missing: {emotion}"
    assert f"case Emotion::{emotion}:" in source, f"Pose missing: {emotion}"
    assert re.search(r'\{"[a-z]+",\s*Emotion::' + emotion + r'\}', serial), (
        f"Serial command missing: {emotion}"
    )

glyphs = {int(code, 16) for code in re.findall(r"\{0x([0-9A-F]{4}),\s*\{", font)}
phrases = re.findall(r'return u8"([^"]+)";', source)
assert len(phrases) == len(emotions), "Every emotion needs one cheeky comic phrase"
persona_phrases = re.findall(
    r'static const char k(?:Calm|Playful)[A-Za-z]+\[\] = u8"([^"]+)";',
    source,
)
assert len(persona_phrases) == 6, "Expected six safe/playful dialogue variants"
phrases += persona_phrases

for phrase in phrases:
    missing = sorted({char for char in phrase if char != " " and ord(char) not in glyphs})
    assert not missing, f"Missing comic glyphs {missing!r} in {phrase!r}"
    assert len(phrase) * 8 + 18 <= 190, f"Comic bubble too wide: {phrase!r}"

assert "bool sassEnabled_ = false;" in face_header, "Profanity must default to OFF"
assert "if (!sassEnabled_ || !quipVisible_ || sleeping_)" in source
for command in ("sass on", "sass off", "quip", "demo on", "demo off"):
    assert f'strcmp(command, "{command}")' in serial, f"Missing: {command}"
assert "const bool diagonal" in source, "Dizzy pupils must visibly animate"
assert "kSpecialTransitionMs" in source, "Missing blink transitions"
assert "case Emotion::Bewildered:" in source
assert "visualEmotion_ == Emotion::Bewildered" in source
assert "bewilderedOffset" in source, "Missing delayed pupil travel"
assert "4800u" in source, "Missing return cycle of the drifting pupil"
assert "mouthY + 3 + length" in source, "Missing animated drool drop"
assert '"bewildered", Emotion::Bewildered' in serial, "Missing Serial command"
assert "cycling all 17 emotions" in serial, "Wrong demo description"


reaction = (ROOT / "include/ReactionController.h").read_text(encoding="utf-8")
assert "kReactionDurationMs = 4500" in reaction
assert "kDefaultIdleSeconds = 120" in reaction
assert "void buildResult(bool success" in reaction
assert "void nextEmotion(uint32_t nowMs)" in reaction
assert 'strcmp(command, "event build-success")' in serial
assert 'strcmp(command, "event build-failure")' in serial
for control in ("idle on", "idle off", "status"):
    assert f'strcmp(command, "{control}")' in serial, f"Missing control: {control}"

ambient = (ROOT / "include/IdleBehavior.h").read_text(encoding="utf-8")
face_cpp = source
assert "enum class IdleGesture" in ambient
for gesture in ("LookLeft", "LookRight", "Squint", "Yawn",
                "Startled", "BrowBounce", "Wink"):
    assert gesture in ambient, f"Missing ambient gesture: {gesture}"
assert "minInterval(personality_)" in ambient
assert "maxInterval(personality_)" in ambient
assert "kCalm[7]" in ambient
assert "kPlayful[7]" in ambient
assert "kCheeky[7]" in ambient
assert "bool tick(uint32_t nowMs, bool eligible)" in ambient
assert "idleGesture_ = gesture;" in face_cpp
assert "float FaceAnimator::idleEnvelope() const" in face_cpp
assert "void FaceAnimator::drawIdleDetails(int eyeY)" in face_cpp
assert "ambient.tick(now, allowAmbient)" in serial
assert "reactions.manualEmotion() == Emotion::Neutral" in serial
assert "reactions.reactionActive()" in serial
assert "reactions.idleActive()" in serial
for command in ("ambient on", "ambient off"):
    assert f'strcmp(command, "{command}")' in serial, f"Missing: {command}"
assert "cancelAmbient(now);" in serial
assert (ROOT / "tests/idle_behavior_test.cpp").is_file()

personality = (ROOT / "include/Personality.h").read_text(encoding="utf-8")
assert "enum class Personality" in personality
for name in ("Calm", "Playful", "Cheeky"):
    assert name in personality
    assert f"Personality::{name}" in ambient
    assert f"Personality::{name}" in source
assert "Personality personality_ = Personality::Calm;" in face_header
assert "Personality personality_ = Personality::Calm;" in ambient
assert "const char* FaceAnimator::quipForEmotion(Emotion emotion) const" in source
assert 'strncmp(command, "personality ", 12)' in serial
assert "ambient.setPersonality(next, nowMs);" in serial
assert "face.setPersonality(next, nowMs);" in serial
assert 'Serial.print(personalityName(ambient.personality()))' in serial
assert (ROOT / "tests/personality_test.cpp").is_file()

heart = (ROOT / "include/HeartShape.h").read_text(encoding="utf-8")
assert "constexpr Row kRows[]" in heart, "Heart silhouette table missing"
assert "template <typename DrawRect>" in heart
assert "void FaceAnimator::drawHeartEye(int centerX, int centerY, float openness)" in source
assert "drawHeartEye(centerX, centerY + 1, eyeOpen);" in source
assert 'kLoveRed = 0xFA8D' in source, "Love must use coral-red RGB565 #FF5268"
assert 'kLoveHighlight = 0xFE7B' in source
assert "canvas_.fillRect(x, y, w, h, kLoveRed);" in source
assert "visualEmotion_ == Emotion::Love ? kLoveRed : kAccent" in source
assert (ROOT / "tests/heart_shape_test.cpp").is_file()

palette = (ROOT / "include/MoodPalette.h").read_text(encoding="utf-8")
assert "kAngryEye = 0xFE91" in palette, "Angry must use amber #FFD18B"
assert "kAngryBrow = 0xFCC6" in palette
assert "kFuriousEye = 0xF9CA" in palette, "Furious must use scarlet #FF3B55"
assert "kFuriousBrow = 0xF929" in palette
assert "kFuriousSpark = 0xFEAB" in palette
assert "inline uint16_t blend565(" in palette
assert "inline Colors colors(" in palette
assert "void FaceAnimator::animateMoodColors()" in source
assert "angerTint_ += (angryTarget - angerTint_) * 0.19f;" in source
assert "furyTint_ += (furiousTarget - furyTint_) * 0.19f;" in source
assert "deskpet_mood::colors(angryStrength, furiousStrength, baseBrow)" in source
assert "canvas_.fillRoundRect(left, top, eyeW, visibleH, radius, eyeFill_);" in source
assert "canvas_.fillCircle(pupilX, pupilY, pupilRadius, pupilFill_);" in source
assert "visualEmotion_ == Emotion::Angry" in source
assert "visualEmotion_ == Emotion::Furious" in source
assert "kLoveRed = 0xFA8D" in source, "Love colour must remain unchanged"
assert (ROOT / "tests/mood_palette_test.cpp").is_file()

glow = (ROOT / "include/FuryGlow.h").read_text(encoding="utf-8")
assert "constexpr uint32_t kPulsePeriodMs = 1200u;" in glow
assert "constexpr uint32_t kIntroFlashMs = 320u;" in glow
assert "inline Halo alpha(uint32_t elapsedMs, uint8_t strength)" in glow
assert "template <typename Stroke>" in glow
assert "const int offsets[] = {13, 9, 5};" in glow
assert "deskpet_mood::blend565(" in glow
assert '#include "FuryGlow.h"' in source
assert "if (furious && !sleeping_ && eyeOpen > 0.30f)" in source
assert "deskpet_fury_glow::draw(" in source
assert "canvas_.drawRoundRect(x, y, w, h, r, color);" in source
assert "animationNowMs_ - emotionStartedMs_, strength, kBackground" in source
assert "kLoveRed = 0xFA8D" in source
assert (ROOT / "tests/fury_glow_test.cpp").is_file()

particles = (ROOT / "include/EmotionParticles.h").read_text(encoding="utf-8")
assert "constexpr uint8_t kParticleCount = 6;" in particles
assert "inline Particle sample(Style style, uint32_t elapsedMs, uint8_t index)" in particles
for emotion, style in (("Excited", "Sparks"), ("Love", "Hearts"),
                       ("Furious", "Steam")):
    assert f"case Emotion::{emotion}: return Style::{style};" in particles
assert "opacity" in particles and "kCycleMs = 2400u;" in particles
assert '#include "EmotionParticles.h"' in source
assert "void FaceAnimator::drawParticles(float openness)" in source
assert "drawParticles(openness);" in source
assert "if (sleeping_ || openness <= 0.12f) return;" in source
assert "deskpet_particles::sample(style, elapsed, index)" in source
assert "kHeartPixels[6]" in source
assert "canvas_.drawLine(x - 2, y + 3, x + 1, y + 1, color)" in source
assert (ROOT / "tests/emotion_particles_test.cpp").is_file()

lifecycle = (ROOT / "include/FaceLifecycle.h").read_text(encoding="utf-8")
assert "class Timeline" in lifecycle
for phase in ("Booting", "Awake", "FallingAsleep", "Asleep", "Waking"):
    assert phase in lifecycle, f"Missing lifecycle phase: {phase}"
assert "kBootDurationMs = 1600u" in lifecycle
assert "kSleepDurationMs = 850u" in lifecycle
assert "kWakeDurationMs = 1050u" in lifecycle
assert "startOpening_ = openness(nowMs);" in lifecycle, "Interrupted transitions must not jump"
assert "float openness(uint32_t nowMs) const" in lifecycle
assert "inline const char* phaseName(Phase phase)" in lifecycle
assert "face.lifecycleName()" in serial
assert "deskpet_lifecycle::Timeline lifecycle_;" in face_header
assert "return lifecycle_.sleeping();" in face_header
assert "lifecycle_.begin(now);" in source
assert "lifecycle_.advance(nowMs);" in source
assert "lifecycle_.sleep(nowMs);" in source
assert "lifecycle_.wake(nowMs);" in source
assert "lifecycle_.interruptBoot(nowMs);" in source
assert "lifecycle_.openness(nowMs) * (1.0f - blink * 0.94f)" in source
assert "if (eyeOpen < 0.14f)" in source, "Sleep should close eyes gradually"
assert "if (sleeping_ || eyeOpen < 0.14f)" not in source
assert "void FaceAnimator::drawLifecycleOverlay()" in source
assert "drawLifecycleOverlay();" in source
assert "canvas_.print(\"HELLO!\");" in source
assert 'canvas_.print("z");' in source
assert 'canvas_.print("Z");' in source
assert 'canvas_.print("HI!");' in source
assert (ROOT / "tests/face_lifecycle_test.cpp").is_file()

print(f"PASS: {len(emotions)} emotions, {len(phrases)} Russian phrases, "
      f"{len(glyphs)} font glyphs; sass opt-in and Serial controls")
