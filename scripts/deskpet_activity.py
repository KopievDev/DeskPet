#!/usr/bin/env python3
"""Optional macOS-only DeskPet presence helper.

Reads Apple's aggregate HIDIdleTime property (time since last interaction).
Never reads keys, titles, windows, app names or text. Only sends transitions
over an explicitly selected USB serial port. No pip dependencies, no network.

Examples:
  python3 scripts/deskpet_activity.py --simulate 0,60,100,120,1
  python3 scripts/deskpet_activity.py --port /dev/cu.usbmodem101 --idle-seconds 90
"""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import time


IDLE_RE = re.compile(r'"HIDIdleTime"\s*=\s*(\d+)')


class ActivityDetector:
    """Two-state debouncer; no repeated Serial messages while state holds."""

    def __init__(self, idle_seconds: int = 90) -> None:
        if idle_seconds < 10:
            raise ValueError("idle threshold must be at least 10 seconds")
        self.threshold = idle_seconds
        self.last_idle = None

    def observe(self, seconds: float):
        idle = seconds >= self.threshold
        if idle == self.last_idle:
            return None
        self.last_idle = idle
        return "event user-idle" if idle else "event user-active"


def system_idle_seconds() -> float:
    """Read IOHIDSystem idle duration (nanoseconds) using built-in ioreg."""
    completed = subprocess.run(
        ["/usr/sbin/ioreg", "-c", "IOHIDSystem", "-d", "3"],
        capture_output=True, text=True, timeout=8, check=True,
    )
    match = IDLE_RE.search(completed.stdout)
    if match is None:
        raise RuntimeError("HIDIdleTime was not reported by macOS ioreg")
    return int(match.group(1)) / 1_000_000_000


def open_serial(path: str) -> int:
    """Configure Mac USB serial using its built-in stty, then open once."""
    port = Path(path)
    if not str(port).startswith("/dev/cu.") and not str(port).startswith("/dev/tty."):
        raise ValueError("port must be a macOS /dev/cu.* or /dev/tty.* device")
    subprocess.run(
        ["/bin/stty", "-f", str(port), "115200", "raw", "-echo"],
        capture_output=True, text=True, check=True, timeout=8,
    )
    return os.open(str(port), os.O_WRONLY | os.O_NOCTTY)


def send(fd: int, command: str) -> None:
    data = (command + "\n").encode("ascii")
    while data:
        size = os.write(fd, data)
        if size <= 0:
            raise OSError("Serial device stopped accepting data")
        data = data[size:]


def run() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="ESP32 serial port, e.g. /dev/cu.usbmodem101")
    parser.add_argument("--idle-seconds", type=int, default=90,
                        help="Start idle reaction after this many seconds (min 10)")
    parser.add_argument("--interval", type=float, default=4.0,
                        help="How often to check activity (seconds, min 1)")
    parser.add_argument("--simulate", metavar="SECONDS,...",
                        help="Dry-run sample HID idle times without opening a port")
    args = parser.parse_args()
    if sys.platform != "darwin" and args.simulate is None:
        parser.error("Live activity monitoring requires macOS")
    if args.interval < 1.0:
        parser.error("--interval must be >= 1 second")
    try:
        detector = ActivityDetector(args.idle_seconds)
    except ValueError as exc:
        parser.error(str(exc))

    if args.simulate is not None:
        for raw in args.simulate.split(","):
            try:
                seconds = float(raw.strip())
            except ValueError:
                parser.error(f"Not a number: {raw!r}")
            command = detector.observe(seconds)
            print(f"idle={seconds:g}s -> {command or '(unchanged)'}")
        return 0

    if not args.port:
        parser.error("--port is required unless --simulate is used")

    try:
        fd = open_serial(args.port)
    except (OSError, subprocess.SubprocessError, ValueError) as exc:
        print(f"Cannot open serial device: {exc}", file=sys.stderr)
        return 2

    print(f"DeskPet macOS activity helper: port={args.port}, idle>={args.idle_seconds}s")
    print("Only ACTIVE/AWAY transitions are sent; no keystrokes or text are collected.")
    print("Stop with Ctrl+C. Close PlatformIO Serial Monitor before starting.")
    try:
        # On some USB CDC boards, opening the serial port resets firmware.
        # Give it time to boot before the first command.
        time.sleep(2.5)
        while True:
            idle_seconds = system_idle_seconds()
            command = detector.observe(idle_seconds)
            if command:
                send(fd, command)
                print(f"{command} (mac idle {idle_seconds:.0f}s)", flush=True)
            time.sleep(args.interval)
    except KeyboardInterrupt:
        print("\nActivity helper stopped.")
        return 0
    except (OSError, subprocess.SubprocessError, RuntimeError) as exc:
        print(f"Activity monitoring stopped: {exc}", file=sys.stderr)
        return 1
    finally:
        os.close(fd)


if __name__ == "__main__":
    raise SystemExit(run())
