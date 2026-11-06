import os
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "build_files" / "55-generate-initramfs.sh"


def _write_exe(path: Path, body: str) -> None:
    path.write_text(body)
    path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


class InitramfsRequirementFailureTest(unittest.TestCase):
    def test_initramfs_requirement_failure_aborts_before_dracut(self):
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            bindir = tmp / "bin"
            bindir.mkdir()
            dracut_marker = tmp / "dracut_marker"

            _write_exe(bindir / "ls", "#!/bin/sh\necho test-kver\n")
            _write_exe(bindir / "mkdir", "#!/bin/sh\nexit 0\n")
            _write_exe(bindir / "python3", "#!/bin/sh\nexit 23\n")
            _write_exe(
                bindir / "dracut",
                '#!/bin/sh\n'
                'printf "%s" "$DRACUT_MARKER" > "${DRACUT_MARKER}.path" 2>/dev/null || true\n'
                'printf "1" > "$DRACUT_MARKER"\n'
                'exit 0\n',
            )

            env = os.environ.copy()
            env["PATH"] = f"{bindir}:{env.get('PATH', '')}"
            env["TMPDIR"] = str(tmp)
            env["DRACUT_MARKER"] = str(dracut_marker)

            proc = subprocess.run(
                ["/bin/bash", str(SCRIPT)],
                cwd=str(tmp),
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=60,
            )

            self.assertEqual(proc.returncode, 23, proc.stderr)
            self.assertFalse(dracut_marker.exists())


if __name__ == "__main__":
    unittest.main()
