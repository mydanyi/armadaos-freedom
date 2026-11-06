#!/usr/bin/env python3
"""Staging-behavior tests for the infinity kernel overlay.

These tests do NOT build a kernel and do NOT read source for prose. They
exercise only the runtime staging/guard behavior by running the extracted
shell block from build-kernel.sh in a sandboxed fake STAGE directory.

Run:  python3 -m unittest tests/test_infinity_package.py
"""

import os
import re
import shutil
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
BUILD_SCRIPT = REPO_ROOT / "scripts" / "build-kernel.sh"

# 7.2.6 version fixtures used across the staging tests.
KVER_OK = "7.2.6-freedom-infinity"
KVER_BAD = "7.2.6-armada"


def _read(path):
    return path.read_text(encoding="utf-8")


def _staging_block():
    """Return the shell snippet that stages the capability marker."""
    src = _read(BUILD_SCRIPT)
    m = re.search(
        r"# >>> freedom-scheduler-capability >>>\n(.*?)\n# <<< freedom-scheduler-capability <<<",
        src,
        re.DOTALL,
    )
    if not m:
        raise AssertionError(
            "build-kernel.sh is missing the freedom-scheduler-capability block"
        )
    return m.group(1)


def _run_staging_block(tmpdir, overlays, kver):
    """Run the extracted staging block in a fake STAGE directory."""
    block = _staging_block()
    stage = Path(tmpdir) / "stage"
    (stage / "lib" / "modules" / kver).mkdir(parents=True)
    # Assign variables via env= so whitespace/tabs in overlays arrive as
    # literal bytes instead of Python repr escapes that the shell would
    # interpret differently. The script itself only sets bash options and
    # runs the extracted staging block.
    script = textwrap.dedent(
        """
        set -euo pipefail
        {block}
        """
    ).format(block=block)
    sh = Path(tmpdir) / "run.sh"
    sh.write_text(script, encoding="utf-8")
    env = dict(os.environ)
    env.update(STAGE=str(stage), KVER=kver, KERNEL_OVERLAYS=overlays)
    proc = subprocess.run(
        ["bash", str(sh)], capture_output=True, text=True, env=env)
    marker = stage / "lib" / "modules" / kver / "freedom-scheduler"
    content = marker.read_text(encoding="utf-8") if marker.exists() else None
    return proc, content, stage


class StagingCapabilityTests(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp(prefix="infinity-staging-")

    def tearDown(self):
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def test_default_build_has_no_marker(self):
        proc, content, _ = _run_staging_block(self.tmpdir, overlays="", kver=KVER_BAD)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIsNone(content, "default build must not create freedom-scheduler marker")

    def test_other_overlay_has_no_marker(self):
        proc, content, _ = _run_staging_block(
            self.tmpdir, overlays="example", kver=KVER_BAD
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIsNone(
            content,
            "non-infinity overlays must not create freedom-scheduler marker",
        )

    def test_infinity_overlay_writes_marker(self):
        proc, content, _ = _run_staging_block(
            self.tmpdir, overlays="infinity", kver=KVER_OK
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(
            content, "infinity-v1\n", "marker must contain exactly 'infinity-v1'"
        )

    def test_infinity_marker_with_extra_overlays(self):
        proc, content, _ = _run_staging_block(
            self.tmpdir,
            overlays="example infinity",
            kver=KVER_OK,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(content, "infinity-v1\n")

    def test_infinity_with_whitespace_separated_token_writes_marker(self):
        # Iterating via the shell word-split of KERNEL_OVERLAYS must treat a
        # tab- and multi-space-separated token set as equivalent to single
        # spaces; the normalized token set contains ``infinity``.
        proc, content, _ = _run_staging_block(
            self.tmpdir,
            overlays="example\tinfinity",
            kver=KVER_OK,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(content, "infinity-v1\n")

    def test_infinity_extra_no_match(self):
        # ``infinity-extra`` must NOT match the ``infinity`` token.
        proc, content, _ = _run_staging_block(
            self.tmpdir, overlays="infinity-extra", kver=KVER_BAD
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIsNone(
            content,
            "infinity-extra must not be treated as the infinity overlay",
        )

    def test_infinity_overlay_rejects_wrong_kernelrelease(self):
        proc, content, _ = _run_staging_block(
            self.tmpdir, overlays="infinity", kver=KVER_BAD
        )
        self.assertNotEqual(
            proc.returncode,
            0,
            "infinity overlay must reject kernelrelease %r not ending in -freedom-infinity"
            % KVER_BAD,
        )
        self.assertIsNone(
            content, "marker must not be written when kernelrelease guard fails"
        )

    def test_infinity_with_whitespace_separated_token_rejects_wrong_kernelrelease(self):
        # The guard must apply when the token is reached via whitespace
        # splitting, not only when the overlay list is exactly ``infinity``.
        proc, content, _ = _run_staging_block(
            self.tmpdir,
            overlays="example\t\tinfinity",
            kver=KVER_BAD,
        )
        self.assertNotEqual(proc.returncode, 0)
        self.assertIsNone(content)

    def test_marker_path_is_under_lib_modules_kver(self):
        proc, content, stage = _run_staging_block(
            self.tmpdir, overlays="infinity", kver=KVER_OK
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        expected = stage / "lib" / "modules" / KVER_OK / "freedom-scheduler"
        self.assertTrue(expected.is_file())


if __name__ == "__main__":
    unittest.main()
