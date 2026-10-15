#!/usr/bin/env bash
# Run a real command-line Xcode build and report its exit code to DeskPet.
# Opt in by setting DESKPET_PORT=/dev/cu.usbmodemXXXX.
# This does NOT monitor builds started inside the Xcode GUI.
set -u

if [[ "$#" -eq 0 ]]; then
  echo "Usage: DESKPET_PORT=/dev/cu.usbmodemXXXX bash scripts/xcodebuild-with-deskpet.sh <xcodebuild arguments>" >&2
  exit 2
fi

xcodebuild "$@"
result=$?

if [[ -n "${DESKPET_PORT:-}" ]]; then
  if [[ "$result" -eq 0 ]]; then
    event="event build-success"
  else
    event="event build-failure"
  fi

  # stty -f is macOS/BSD. Use the same 115200 baud rate as the firmware.
  # The serial event is best effort; never alter xcodebuild's exit status.
  if stty -f "$DESKPET_PORT" 115200 raw -echo >/dev/null 2>&1 &&
     printf '%s\n' "$event" > "$DESKPET_PORT"; then
    echo "[DeskPet] $event sent to $DESKPET_PORT" >&2
  else
    echo "[DeskPet] Could not send event; check USB port and close the serial monitor." >&2
  fi
fi

exit "$result"
