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
assert len(emotions) == 16, f"Expected 16 emotions; got {len(emotions)}"

for emotion in emotions:
    assert f"case Emotion::{emotion}: return" in source, f"Name missing: {emotion}"
    assert f"case Emotion::{emotion}:" in source, f"Pose missing: {emotion}"
    assert re.search(r'\{"[a-z]+",\s*Emotion::' + emotion + r'\}', serial), (
        f"Serial command missing: {emotion}"
    )

glyphs = {int(code, 16) for code in re.findall(r"\{0x([0-9A-F]{4}),\s*\{", font)}
phrases = re.findall(r'return u8"([^"]+)";', source)
assert len(phrases) == len(emotions), "Every emotion needs one comic phrase"

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

reaction = (ROOT / "include/ReactionController.h").read_text(encoding="utf-8")
assert "kReactionDurationMs = 4500" in reaction
assert "kDefaultIdleSeconds = 120" in reaction
assert "void buildResult(bool success" in reaction
assert "void nextEmotion(uint32_t nowMs)" in reaction
assert 'strcmp(command, "event build-success")' in serial
assert 'strcmp(command, "event build-failure")' in serial
for control in ("idle on", "idle off", "status"):
    assert f'strcmp(command, "{control}")' in serial, f"Missing control: {control}"

print(f"PASS: {len(emotions)} emotions, {len(phrases)} Russian quips, "
      f"{len(glyphs)} font glyphs; sass opt-in and Serial controls")
