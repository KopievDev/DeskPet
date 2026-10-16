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
assert len(emotions) == 21, f"Expected 21 emotions; got {len(emotions)}"

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
assert "cycling all 21 emotions" in serial, "Wrong demo description"


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
# The bond cameo may start a transient reaction in the same loop tick.
# In that case ambient must be gated again, rather than overriding the cameo.
assert re.search(
    r"ambient\.tick\(\s*now\s*,\s*allowAmbient\s*&&\s*"
    r"!reactions\.reactionActive\(\)\s*&&\s*"
    r"userActivity\.presence\(\)\s*!=\s*"
    r"deskpet_activity::Presence::Away\s*\)",
    serial,
), "Ambient must not override build, PetBond or user-away comedy"
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

gestures = (ROOT / "include/ButtonGesture.h").read_text(encoding="utf-8")
assert "enum class Action" in gestures
for action in ("Tap", "DoubleTap", "Hold"):
    assert action in gestures
assert "kDebounceMs = 35u" in gestures
assert "kHoldMs = 850u" in gestures
assert "kDoubleTapMs = 320u" in gestures
assert "longFired_ = pressed;" in gestures, "BOOT hold during reset must not fire"
assert "pendingTap_ = false; // holding never triggers a short tap" in gestures
assert "Action poll(bool pressed, uint32_t nowMs)" in gestures
assert '#include "ButtonGesture.h"' in serial
assert "deskpet_button::Gesture userButton(true);" in serial
assert "deskpet_button::Gesture bootButton(false);" in serial
assert "void handleUserGesture(deskpet_button::Action action" in serial
assert "void handleBootGesture(deskpet_button::Action action" in serial
assert "reactions.showTemporary(Emotion::Love, nowMs, kPetDurationMs)" in serial
assert "reactions.showTemporary(Emotion::Surprised, nowMs, kSurpriseDurationMs)" in serial
assert "face.setSleeping(true, nowMs)" in serial
assert "face.setSleeping(false, nowMs)" in serial
assert "ambient.setPersonality(next, nowMs)" in serial
assert "face.setPersonality(next, nowMs)" in serial
assert "static constexpr uint32_t kReactionDurationMs = 4500" in reaction
assert "void showTemporary(Emotion emotion" in reaction
assert "reactionDurationMs_" in reaction
assert (ROOT / "tests/button_gesture_test.cpp").is_file()

# Comic-expression regression: recognisable accessories on tiny IPS.
for mood in ("Nerd", "Derp", "Gremlin", "Panic"):
    assert f"case Emotion::{mood}:" in source, f"No face pose for {mood}"
    assert f"Emotion::{mood}" in serial, f"No command for {mood}"
    assert f"emotion == Emotion::{mood}" in source, f"No blink transition for {mood}"
assert "void FaceAnimator::drawComicFeatures(int eyeY, float openness)" in source
assert "drawComicFeatures(eyeY, openness);" in source
for visual in ("canvas_.drawRoundRect(x, eyeY",
               "canvas_.fillRect(mid - 9, eyeY + 30",
               "canvas_.fillRoundRect(mid + 2 + sway",
               "canvas_.fillTriangle(mid - 15",
               "canvas_.fillTriangle(sweatX, sweatY - 4"):
    assert visual in source, f"Comic feature missing: {visual}"

bond = (ROOT / "include/PetBond.h").read_text(encoding="utf-8")
assert "class PetBond" in bond
assert "kInitialScore = 50u" in bond
assert "kDecayMs = 600000u" in bond
assert "void pet(uint32_t nowMs)" in bond
assert "void interact(uint32_t nowMs)" in bond
assert "bool tick(uint32_t nowMs, bool awake, bool eligible" in bond
assert "Emotion::Nerd" in bond and "Emotion::Derp" in bond
assert "Emotion::Gremlin" in bond and "Emotion::Panic" in bond
assert "PetBond bond;" in serial
assert "bond.pet(nowMs);" in serial
assert "bond.surprise(nowMs);" in serial
assert "bond.build(success, nowMs);" in serial
# Bond must inherit the stronger Needs eligibility gate, which itself
# includes Ambient's sleep/demo/manual-emotion exclusions.
assert re.search(r"const bool allowNeeds\s*=\s*allowAmbient\s*&&", serial)
assert re.search(r"const bool allowBond\s*=\s*allowNeeds\s*&&", serial)
assert "!reactions.reactionActive() &&" in serial
assert "userActivity.presence() != deskpet_activity::Presence::Away" in serial
assert "bond.tick(now, !face.isSleeping(), allowBond" in serial
assert "reactions.showTemporary(comic, now, PetBond::kComicDurationMs)" in serial
assert "face.setAffection(bond.score());" in serial
assert "bond.status" not in serial  # status command is textual: bond status
for cmd in ("bond on", "bond off", "bond reset", "bond status"):
    assert f'strcmp(command, "{cmd}")' in serial
assert (ROOT / "tests/pet_bond_test.cpp").is_file()
assert "checkMood(reactions, Emotion::Panic);" in (
    ROOT / "tests/reaction_controller_test.cpp"
).read_text(encoding="utf-8")

skits = (ROOT / "include/ComicSkits.h").read_text(encoding="utf-8")
assert "constexpr uint32_t kSceneDurationMs = 4800u" in skits
assert "inline Frame at(Emotion mood, uint32_t elapsedMs)" in skits
assert "inline uint32_t lerpEnvelope(" in skits
assert "sweatAlpha" in skits and "const uint32_t drip = t % 1600u;" in skits
for mood in ("Nerd", "Derp", "Gremlin", "Panic"):
    assert f"case Emotion::{mood}:" in skits, f"Missing acting scene {mood}"
assert '#include "ComicSkits.h"' in source
assert "deskpet_comics::at(" in source
assert "skit.glassesLift" in source
assert "skit.tongueDrop" in source
assert "skit.grinLift" in source
assert "skit.jawShake" in source
assert "skit.sweatAlpha" in source
assert 'strcmp(command, "comic next")' in serial
assert 'Serial.println("Comic: ");' not in serial  # use print, not println
assert (ROOT / "tests/comic_skits_test.cpp").is_file()

build_story = (ROOT / "include/BuildComedy.h").read_text(encoding="utf-8")
assert "class Story" in build_story
assert "kStreakWindowMs = 600000u" in build_story
assert "kComicDurationMs = 4800u" in build_story
assert "Cue record(bool success, uint32_t nowMs)" in build_story
for mood in ("Panic", "Gremlin", "Furious", "Nerd", "Excited"):
    assert f"Emotion::{mood}" in build_story
assert "if (failures_ < 99u) ++failures_;" in build_story
assert "static_cast<uint32_t>(nowMs - lastFailureMs_)" in build_story
assert '#include "BuildComedy.h"' in serial
assert "deskpet_build_comedy::Story buildStory;" in serial
assert "buildStory.begin();" in serial
assert "buildStory.record(success, nowMs)" in serial
assert "reactions.showTemporary(cue.emotion, nowMs, cue.durationMs)" in serial
assert "bond.build(success, nowMs);" in serial
assert "if (face.isSleeping()) {" in serial  # sleeping builds remain ignored
assert 'strcmp(command, "build status")' in serial
assert 'strcmp(command, "build reset")' in serial
assert 'Serial.print(buildStory.consecutiveFailures());' in serial
assert (ROOT / "tests/build_comedy_test.cpp").is_file()

user_activity = (ROOT / "include/UserActivity.h").read_text(encoding="utf-8")
assert "class Controller" in user_activity
assert "enum class Presence" in user_activity
assert "kIdleJokeIntervalMs = 90000u" in user_activity
assert "kPendingLifetimeMs = 12000u" in user_activity
assert "void idle(uint32_t nowMs)" in user_activity
assert "void active(uint32_t nowMs)" in user_activity
assert "bool tick(uint32_t nowMs, bool eligible, Personality personality," in user_activity
assert "firstHostEvent ? Pending::None : Pending::Idle" in user_activity
assert "Presence::Away" in user_activity
assert '#include "UserActivity.h"' in serial
assert "deskpet_activity::Controller userActivity;" in serial
assert "userActivity.begin(startedAt);" in serial
assert 'strcmp(command, "event user-idle")' in serial
assert 'strcmp(command, "event user-active")' in serial
assert 'strcmp(command, "activity on")' in serial
assert 'strcmp(command, "activity off")' in serial
assert 'strcmp(command, "activity status")' in serial
assert "userActivity.tick(now, allowActivity, ambient.personality()," in serial
assert "reactions.showTemporary(activityCue.emotion, now," in serial
assert (ROOT / "tests/user_activity_test.cpp").is_file()

helper = (ROOT.parent / "scripts/deskpet_activity.py").read_text(encoding="utf-8")
assert '"HIDIdleTime"' in helper
assert "event user-idle" in helper
assert "event user-active" in helper
assert "--simulate" in helper
assert (ROOT.parent / "scripts/test_deskpet_activity.py").is_file()

needs = (ROOT / "include/PetNeeds.h").read_text(encoding="utf-8")
assert "namespace deskpet_needs" in needs
assert "enum class Kind : uint8_t" in needs
assert "class Controller" in needs
assert "kEnergyAwakeMs = 240000u" in needs
assert "kEnergySleepMs = 60000u" in needs
assert "kAttentionMs = 240000u" in needs
assert "kMoodMs = 540000u" in needs
assert "kCueIntervalMs = 90000u" in needs
assert "void advance(uint32_t nowMs, bool sleeping)" in needs
assert "if (sleeping != lastSleeping_)" in needs
assert "uint8_t energy() const" in needs
assert "uint8_t mood() const" in needs
assert "uint8_t attention() const" in needs
assert "void pet()" in needs and "void coffee()" in needs
assert "void play()" in needs and "void build(bool success)" in needs
assert "bool tick(uint32_t nowMs, bool eligible, Personality personality," in needs
assert "Emotion::Nerd, Kind::Coffee" in needs
assert "Kind::Attention" in needs and "Kind::Cheer" in needs
assert '#include "PetNeeds.h"' in serial
assert "deskpet_needs::Controller needs;" in serial
assert "needs.begin(startedAt);" in serial
assert "needs.advance(now, face.isSleeping());" in serial
assert "needs.pet();" in serial
assert "needs.interact();" in serial
assert "needs.surprise();" in serial
assert "needs.build(success);" in serial
assert "needs.tick(now, allowNeeds, ambient.personality(), needsCue)" in serial
assert "reactions.showTemporary(needsCue.emotion, now, needsCue.durationMs)" in serial
assert "face.setNeedPrompt(needsCue.kind);" in serial
assert "face.setNeeds(needs.energy(), needs.mood(), needs.attention());" in serial
for command in ("needs status", "needs reset", "needs on", "needs off",
                "needs coffee", "needs play", "needs set ",
                "needs show "):
    assert command in serial, f"Missing needs command: {command}"
assert "void FaceAnimator::drawNeedPrompt(int eyeY, float openness)" in source
assert "drawNeedPrompt(eyeY, openness);" in source
assert "canvas_.drawRoundRect(x, y, 17, 13, 3, kCup);" in source
assert "needPrompt_ == deskpet_needs::Kind::None" in source
assert "canvas_.print(energy_);" in source
assert "canvas_.print(mood_);" in source
assert "canvas_.print(attention_);" in source
assert "void setNeedPrompt(deskpet_needs::Kind prompt)" in face_header
assert (ROOT / "tests/pet_needs_test.cpp").is_file()

wardrobe = (ROOT / "include/AccessoryWardrobe.h").read_text(encoding="utf-8")
assert "namespace deskpet_accessories" in wardrobe
assert "class Wardrobe" in wardrobe
assert "inline Outfit automaticOutfit(" in wardrobe
assert "void surprise(bool crownUnlocked = false" in wardrobe
assert "void tick(Emotion emotion, Personality personality," in wardrobe
assert "if (sleeping || mode_ == Mode::Off)" in wardrobe
assert "void equip(Item item)" in wardrobe
assert '#include "AccessoryWardrobe.h"' in serial
assert "deskpet_accessories::Wardrobe wardrobe;" in serial
assert "wardrobe.tick(reactions.desiredEmotion(), ambient.personality()," in serial
assert "deskpet_accessories::Outfit currentOutfit = wardrobe.outfit();" in serial
assert "face.setAccessories(currentOutfit);" in serial
assert "void FaceAnimator::drawAccessories(int eyeY, float openness)" in source
assert "drawAccessories(eyeY, openness);" in source
assert "deskpet_accessories::Outfit accessories_" in face_header
assert "if (sleeping_ || openness <= 0.35f) return;" in source
for command in ("inventory", "wear status", "wear auto", "wear on",
                "wear off", "wear clear", "wear random"):
    assert f'strcmp(command, "{command}")' in serial, (
        f"Missing wardrobe command: {command}"
    )
assert 'strncmp(command, "wear ", 5)' in serial
for item in ("beanie", "partyhat", "glasses", "mustache", "bowtie",
             "crown", "shades", "starpin"):
    assert f'strcmp(name, "{item}")' in serial, (
        f"Missing wearable item command: {item}"
    )
assert (ROOT / "tests/accessory_wardrobe_test.cpp").is_file()

finds = (ROOT / "include/PetFinds.h").read_text(encoding="utf-8")
assert "class Collection" in finds
assert "kCooldownMs = 60000u" in finds
assert "Prize reward(Action action, uint32_t nowMs)" in finds
assert "void reset()" in finds
assert "if (++buildProgress_ == 3u)" in finds
assert "if (++playProgress_ == 4u)" in finds
assert "if (++petProgress_ == 5u)" in finds
assert "uint32_t nextRandom()" in finds
assert '#include "PetFinds.h"' in serial
assert "deskpet_finds::Collection finds;" in serial
assert "finds.begin(" in serial
assert "finds.reward(deskpet_finds::Action::BuildSuccess," in serial
assert "finds.reward(deskpet_finds::Action::Play, nowMs)" in serial
assert "finds.reward(deskpet_finds::Action::Pet, nowMs)" in serial
assert "finds.owns(deskpet_finds::Prize::Crown)" in serial
assert "face.setFindNotice(accessoryFor(prize), nowMs);" in serial
assert 'strcmp(command, "find status")' in serial
assert 'strcmp(command, "find reset")' in serial
assert 'strncmp(command, "find grant ", 11)' in serial
assert "void FaceAnimator::drawFindNotice()" in source
assert "drawFindNotice();" in source
for item in ("Crown", "Shades", "StarPin"):
    assert f"Item::{item}" in wardrobe
    assert f"Item::{item}" in source
assert (ROOT / "tests/pet_finds_test.cpp").is_file()

progress = (ROOT / "include/PetProgress.h").read_text(encoding="utf-8")
storage = (ROOT / "include/PetProgressStorage.h").read_text(encoding="utf-8")
assert "struct Snapshot" in progress
assert "kVersion = 1u" in progress
assert "kWireSize = 28u" in progress
assert "uint32_t checksum(" in progress
assert "inline bool decode(" in progress
assert "class Journal" in progress
assert "kMinWriteIntervalMs = 60000u" in progress
assert "class Storage" in storage
assert "prefs_.putBytes(" in storage
assert '"saveA"' in storage and '"saveB"' in storage
assert "bool clear()" in storage
assert "deskpet_progress::Storage saveStorage;" in serial
assert "deskpet_progress::Journal saveJournal;" in serial
assert "restoreProgress(restored, startedAt)" in serial
assert "saveJournal.begin(captureProgress(), startedAt, loaded)" in serial
assert "autoSaveProgress(now);" in serial
assert 'strcmp(command, "save status")' in serial
assert 'strcmp(command, "save now")' in serial
assert 'strcmp(command, "save reset confirm")' in serial
assert (ROOT / "tests/pet_progress_test.cpp").is_file()

print(f"PASS: {len(emotions)} emotions, {len(phrases)} Russian phrases, "
      f"{len(glyphs)} font glyphs; sass opt-in and Serial controls")
