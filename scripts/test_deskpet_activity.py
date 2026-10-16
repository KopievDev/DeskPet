#!/usr/bin/env python3
"""Run with: python3 -m unittest discover -s scripts -p 'test_*.py'."""

import unittest
from unittest.mock import patch
import subprocess

from deskpet_activity import ActivityDetector, IDLE_RE, system_idle_seconds


class ActivityHelperTests(unittest.TestCase):
    def test_transitions_only(self):
        detector = ActivityDetector(90)
        self.assertEqual(detector.observe(0), "event user-active")
        self.assertIsNone(detector.observe(80))
        self.assertIsNone(detector.observe(89.99))
        self.assertEqual(detector.observe(90), "event user-idle")
        self.assertIsNone(detector.observe(130))
        self.assertIsNone(detector.observe(90))
        self.assertEqual(detector.observe(5), "event user-active")
        self.assertIsNone(detector.observe(0))

    def test_initially_away(self):
        detector = ActivityDetector(60)
        self.assertEqual(detector.observe(250), "event user-idle")
        self.assertIsNone(detector.observe(280))
        self.assertEqual(detector.observe(2), "event user-active")

    def test_threshold_validation(self):
        with self.assertRaises(ValueError):
            ActivityDetector(9)

    def test_hid_ns_parser(self):
        # Only aggregate HID idle duration is used. No key event data.
        example = '| |   "HIDIdleTime" = 92250000000\n'
        result = IDLE_RE.search(example)
        self.assertIsNotNone(result)
        self.assertEqual(int(result.group(1)) / 1e9, 92.25)

    def test_ioreg_invocation_has_no_keylogging(self):
        output = subprocess.CompletedProcess(
            args=[], returncode=0,
            stdout='    "HIDIdleTime" = 90000000000\n', stderr="",
        )
        with patch("deskpet_activity.subprocess.run", return_value=output) as run:
            self.assertEqual(system_idle_seconds(), 90.0)
        command = run.call_args.args[0]
        self.assertEqual(command, ["/usr/sbin/ioreg", "-c", "IOHIDSystem", "-d", "3"])
        self.assertNotIn("keyboard", " ".join(command).lower())

    def test_missing_idle_time_is_reported(self):
        completed = subprocess.CompletedProcess(
            args=[], returncode=0, stdout="No HID property", stderr="",
        )
        with patch("deskpet_activity.subprocess.run", return_value=completed):
            with self.assertRaises(RuntimeError):
                system_idle_seconds()


if __name__ == "__main__":
    unittest.main()
