import importlib.util
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

REPO_ROOT = Path(__file__).resolve().parents[1]
VERIFIER_PATH = REPO_ROOT / 'build_files' / 'verify_plasma_runtime.py'
TAB = chr(9)
NEWLINE = chr(10)


def _load_verifier():
    spec = importlib.util.spec_from_file_location(
        'verify_plasma_runtime_under_test', VERIFIER_PATH
    )
    if spec is None or spec.loader is None:
        raise RuntimeError(f'cannot load {VERIFIER_PATH}')
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class VerifyPackagesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.verify = _load_verifier()

    def _rpm_stdout(self, versions):
        return ''.join(
            f'{name}{TAB}{versions[name]}{NEWLINE}'
            for name in self.verify.CORE_PACKAGES
            if name in versions
        )

    def _completed(self, returncode=0, stdout='', stderr=''):
        return subprocess.CompletedProcess(['rpm'], returncode, stdout, stderr)

    def _assert_packages_fail(self, stdout, returncode=0, stderr='', message=None):
        completed = self._completed(returncode, stdout, stderr)
        with mock.patch.object(
            self.verify.subprocess, 'run', return_value=completed
        ):
            with self.assertRaises(RuntimeError) as raised:
                self.verify.verify_packages()
        if message is not None:
            self.assertIn(message.lower(), str(raised.exception).lower())

    def test_coherent_core_set_passes(self):
        versions = {name: '6.8.2' for name in self.verify.CORE_PACKAGES}
        completed = self._completed(stdout=self._rpm_stdout(versions))
        with mock.patch.object(
            self.verify.subprocess, 'run', return_value=completed
        ) as run:
            actual = self.verify.verify_packages()
        self.assertEqual(actual, versions)
        command = run.call_args[0][0]
        self.assertEqual(
            command[:5],
            ['rpm', '--root', '/', '--dbpath', '/usr/lib/sysimage/rpm'],
        )
        self.assertEqual(command[5:7], ['-q', '--qf'])
        self.assertEqual(command[8:], list(self.verify.CORE_PACKAGES))

    def test_each_core_version_mismatch_fails(self):
        for package in self.verify.CORE_PACKAGES:
            versions = {name: '6.8.2' for name in self.verify.CORE_PACKAGES}
            versions[package] = '6.9.0'
            with self.subTest(package=package):
                self._assert_packages_fail(
                    self._rpm_stdout(versions), message='disagree'
                )

    def test_missing_package_fails(self):
        versions = {name: '6.8.2' for name in self.verify.CORE_PACKAGES}
        versions.pop('kwin')
        self._assert_packages_fail(
            self._rpm_stdout(versions),
            returncode=1,
            stderr='package kwin is not installed',
            message='missing',
        )

    def test_duplicate_package_fails(self):
        versions = {name: '6.8.2' for name in self.verify.CORE_PACKAGES}
        stdout = self._rpm_stdout(versions) + 'kwin' + TAB + '6.8.2' + NEWLINE
        self._assert_packages_fail(stdout, message='more than once')

    def test_empty_version_fails(self):
        versions = {name: '6.8.2' for name in self.verify.CORE_PACKAGES}
        stdout = self._rpm_stdout(versions).replace(
            'kwin' + TAB + '6.8.2' + NEWLINE,
            'kwin' + TAB + NEWLINE,
        )
        self._assert_packages_fail(stdout, message='empty')

    def test_malformed_rpm_fields_fail(self):
        versions = {name: '6.8.2' for name in self.verify.CORE_PACKAGES}
        good = 'kwin' + TAB + '6.8.2' + NEWLINE
        cases = (
            ('no tab', 'kwin 6.8.2' + NEWLINE, 'malformed'),
            (
                'three fields',
                'kwin' + TAB + '6.8.2' + TAB + 'fc44' + NEWLINE,
                'malformed',
            ),
            ('whitespace version', 'kwin' + TAB + '   ' + NEWLINE, 'empty'),
        )
        for label, replacement, message in cases:
            with self.subTest(case=label):
                stdout = self._rpm_stdout(versions).replace(good, replacement)
                self._assert_packages_fail(stdout, message=message)

    def test_rpm_failure_fails(self):
        versions = {name: '6.8.2' for name in self.verify.CORE_PACKAGES}
        self._assert_packages_fail(
            self._rpm_stdout(versions),
            returncode=1,
            stderr='rpmdb: unable to open database',
            message='rpm package query failed',
        )

    def test_unexpected_package_fails(self):
        versions = {name: '6.8.2' for name in self.verify.CORE_PACKAGES}
        stdout = self._rpm_stdout(versions) + 'plasma-settings' + TAB + '6.8.2' + NEWLINE
        self._assert_packages_fail(stdout, message='unexpected')

    def test_release_independent_when_versions_agree(self):
        versions = {name: '6.9.0' for name in self.verify.CORE_PACKAGES}
        completed = self._completed(stdout=self._rpm_stdout(versions))
        with mock.patch.object(
            self.verify.subprocess, 'run', return_value=completed
        ):
            actual = self.verify.verify_packages()
        self.assertEqual(set(actual.values()), {'6.9.0'})


class VerifyRuntimeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.verify = _load_verifier()

    def _completed(self, returncode=0, stdout='', stderr=''):
        return subprocess.CompletedProcess(
            ['command'], returncode, stdout, stderr
        )

    def test_runtime_refuses_non_root(self):
        with mock.patch.object(self.verify.subprocess, 'run') as run:
            with self.assertRaises(RuntimeError):
                self.verify.verify_runtime(Path('/tmp'))
        run.assert_not_called()

    def _write_kwin_fixture(self, directory, payload):
        original = Path(directory) / 'kwin_wayland.source'
        original.write_bytes(payload)
        return original

    def test_version_check_runs_private_copy_and_no_session(self):
        self.assertEqual(self.verify.KWIN_BINARY, '/usr/bin/kwin_wayland')
        payload = b'private kwin copy' + NEWLINE.encode('ascii')
        results = [
            self._completed(stdout=''),
            self._completed(stdout='kwin 6.8.2' + NEWLINE),
            self._completed(stdout='plasmashell 6.8.2' + NEWLINE),
        ]
        calls = []

        def fake_run(command, **kwargs):
            calls.append((command, kwargs))
            if command[0] not in ('ldd', '/usr/bin/plasmashell'):
                copy = Path(command[0])
                self.assertTrue(copy.is_file())
                self.assertEqual(copy.read_bytes(), payload)
            return results[len(calls) - 1]

        with tempfile.TemporaryDirectory() as directory:
            original = self._write_kwin_fixture(directory, payload)
            with mock.patch.object(
                self.verify, 'KWIN_BINARY', str(original)
            ), mock.patch.object(
                self.verify.subprocess, 'run', side_effect=fake_run
            ):
                self.verify.verify_runtime()
            copied = calls[1][0][0]
            self.assertNotEqual(copied, str(original))
            self.assertFalse(Path(copied).exists())
            self.assertEqual(original.read_bytes(), payload)

        self.assertEqual(len(calls), 3)
        commands = [command for command, _ in calls]
        self.assertEqual(commands[0], ['ldd', '-r', str(original)])
        self.assertEqual(commands[1][1:], ['--version'])
        self.assertEqual(commands[2], ['/usr/bin/plasmashell', '--version'])
        for _, kwargs in calls:
            environment = kwargs['env']
            self.assertEqual(environment['QT_QPA_PLATFORM'], 'minimal')
            self.assertEqual(environment['LD_BIND_NOW'], '1')
            self.assertEqual(environment['QT_FORCE_STDERR_LOGGING'], '1')
            self.assertGreater(kwargs['timeout'], 0)

        rendered = ' '.join(' '.join(command) for command in commands)
        for forbidden in (
            'startplasma',
            'dbus-run-session',
            'systemctl',
            'startx',
        ):
            self.assertNotIn(forbidden, rendered)

    def test_private_copy_removed_after_kwin_failure(self):
        payload = b'kwin failure copy'
        results = [
            self._completed(stdout=''),
            self._completed(returncode=1, stderr='platform plugin failure'),
        ]
        calls = []

        def fake_run(command, **kwargs):
            calls.append(command)
            return results[len(calls) - 1]

        with tempfile.TemporaryDirectory() as directory:
            original = self._write_kwin_fixture(directory, payload)
            with mock.patch.object(
                self.verify, 'KWIN_BINARY', str(original)
            ), mock.patch.object(
                self.verify.subprocess, 'run', side_effect=fake_run
            ):
                with self.assertRaises(RuntimeError) as raised:
                    self.verify.verify_runtime()
            copied = calls[1][0]
            self.assertFalse(Path(copied).exists())
            self.assertEqual(original.read_bytes(), payload)
        self.assertIn('exited 1', str(raised.exception))

    def test_copy_failure_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            original = self._write_kwin_fixture(directory, b'kwin copy')
            with mock.patch.object(
                self.verify, 'KWIN_BINARY', str(original)
            ), mock.patch.object(
                self.verify.subprocess,
                'run',
                side_effect=[self._completed(stdout='')],
            ) as run, mock.patch.object(
                self.verify.shutil,
                'copyfile',
                side_effect=OSError('no space left'),
            ):
                with self.assertRaises(RuntimeError) as raised:
                    self.verify.verify_runtime()
        self.assertIn('could not prepare', str(raised.exception))
        self.assertIn('no space left', str(raised.exception))
        self.assertEqual(run.call_count, 1)

    def test_undefined_symbol_with_zero_exit_fails(self):
        result = self._completed(
            stdout=TAB + 'libexample.so: undefined symbol: example_symbol' + NEWLINE
        )
        with mock.patch.object(
            self.verify.subprocess, 'run', side_effect=[result]
        ) as run:
            with self.assertRaises(RuntimeError) as raised:
                self.verify.verify_runtime()
        self.assertIn('undefined symbol', str(raised.exception).lower())
        self.assertEqual(run.call_count, 1)

    def test_not_found_with_zero_exit_fails(self):
        result = self._completed(
            stdout=TAB + 'libexample.so => not found' + NEWLINE
        )
        with mock.patch.object(
            self.verify.subprocess, 'run', side_effect=[result]
        ):
            with self.assertRaises(RuntimeError) as raised:
                self.verify.verify_runtime()
        self.assertIn('not found', str(raised.exception).lower())

    def test_ldd_nonzero_fails(self):
        result = self._completed(returncode=1, stderr='dynamic loader error')
        with mock.patch.object(
            self.verify.subprocess, 'run', side_effect=[result]
        ) as run:
            with self.assertRaises(RuntimeError) as raised:
                self.verify.verify_runtime()
        self.assertIn('exited 1', str(raised.exception))
        self.assertEqual(run.call_count, 1)

    def test_timeout_fails(self):
        timeout_error = subprocess.TimeoutExpired(
            ['ldd', '-r', '/usr/bin/kwin_wayland'],
            self.verify.LDD_TIMEOUT_SECONDS,
        )
        with mock.patch.object(
            self.verify.subprocess, 'run', side_effect=timeout_error
        ):
            with self.assertRaises(RuntimeError) as raised:
                self.verify.verify_runtime()
        self.assertIn('timed out', str(raised.exception).lower())


class BuildWiringTest(unittest.TestCase):
    def _read(self, relative):
        path = REPO_ROOT / relative
        self.assertTrue(path.is_file(), f'missing {relative}')
        return path.read_text(encoding='utf-8')

    def test_official_sources_use_upstream_fixed_base(self):
        kwin = self._read('packages/kwin/BASE.env')
        powerdevil = self._read('packages/powerdevil/BASE.env')
        self.assertIn('SRPM=kwin-6.8.2-2.fc44', kwin)
        self.assertNotIn('6.7.5', kwin)
        self.assertIn('SRPM=powerdevil-6.8.2-1.fc44', powerdevil)
        self.assertNotIn('6.7.5', powerdevil)

    def test_runtime_verification_is_last_build_step(self):
        lines = [
            line.strip()
            for line in self._read('build_files/build.sh').splitlines()
            if line.strip() and not line.strip().startswith('#')
        ]
        self.assertEqual(
            lines[-1],
            'run_step python3 ./verify_plasma_runtime.py --check-runtime',
        )
        self.assertGreater(
            lines.index('run_step ./80-finalize-update-state.sh'),
            lines.index('run_step ./70-cleanup.sh'),
        )

    def test_edl_producer_checks_plasma_before_model_and_payload(self):
        source = self._read('build_files/make-edl-raw-package.py')
        plasma_call = source.find('        _verify_plasma_packages(deployment)')
        model_call = source.find('        _verify_model_boot_requirements(')
        payload_call = source.find('        _copy_payload(')
        self.assertNotEqual(plasma_call, -1)
        self.assertNotEqual(model_call, -1)
        self.assertNotEqual(payload_call, -1)
        self.assertLess(plasma_call, model_call)
        self.assertLess(plasma_call, payload_call)
        self.assertIn('verify_plasma_runtime_sibling', source)
        self.assertIn('module.verify_packages(Path(deployment))', source)

        helper_start = source.find('def _verify_plasma_packages(deployment):')
        helper_end = source.find(NEWLINE + 'def ', helper_start + 1)
        helper = source[helper_start:helper_end]
        self.assertNotIn('verify_runtime', helper)

    def test_private_plasma_mobile_build_is_removed(self):
        containerfile = self._read('Containerfile')
        packages = self._read('packages/Containerfile')
        base = self._read('build_files/10-base-packages.sh')
        self.assertNotIn('PLASMA_MOBILE_REF', containerfile)
        self.assertNotIn('from=plasma-mobile', containerfile)
        self.assertNotIn('AS plasma-mobile', containerfile)
        self.assertNotIn('pkg-plasma-mobile', packages)
        self.assertNotIn('out-plasma-mobile', packages)
        self.assertNotIn('/packages/plasma-mobile/', base)
        self.assertIn('    plasma-mobile', base)
        self.assertFalse(
            (REPO_ROOT / 'packages' / 'plasma-mobile' / 'build.sh').exists()
        )

    def test_edl_tool_image_has_rpm(self):
        containerfile = self._read('build_files/Containerfile.edl-tools')
        self.assertIn('        rpm', containerfile)

    def test_ci_runs_plasma_runtime_tests(self):
        for relative in (
            '.github/workflows/pr.yml',
            '.github/workflows/build.yml',
            '.github/workflows/build-edl-package.yml',
        ):
            with self.subTest(workflow=relative):
                self.assertIn(
                    'python3 -m unittest discover -s tests -p test_plasma_runtime.py',
                    self._read(relative),
                )


if __name__ == '__main__':
    unittest.main()