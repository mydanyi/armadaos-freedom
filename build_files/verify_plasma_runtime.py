#!/usr/bin/env python3
'''Verify the installed Plasma packages and the KWin runtime agree.'''

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

CORE_PACKAGES = (
    'kwin',
    'kwin-common',
    'kwin-libs',
    'plasma-workspace',
    'plasma-workspace-common',
    'plasma-workspace-libs',
    'plasma-desktop',
    'plasma-mobile',
    'powerdevil',
    'kglobalacceld',
    'kdecoration',
    'plasma-activities',
    'plasma-integration',
    'libplasma',
    'kscreen',
)

RPM_DBPATH = '/usr/lib/sysimage/rpm'
RPM_QUERY_FORMAT = '%{NAME}' + chr(9) + '%{VERSION}' + chr(10)
RPM_TIMEOUT_SECONDS = 180
LDD_TIMEOUT_SECONDS = 300
VERSION_TIMEOUT_SECONDS = 60
KWIN_BINARY = '/usr/bin/kwin_wayland'


def _runtime_environment():
    environment = os.environ.copy()
    environment['QT_QPA_PLATFORM'] = 'minimal'
    environment['LD_BIND_NOW'] = '1'
    environment['QT_FORCE_STDERR_LOGGING'] = '1'
    return environment


def _run(command, description, timeout):
    try:
        return subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=timeout,
            env=_runtime_environment(),
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(
            f'{description} timed out after {timeout} seconds: {command[0]}'
        ) from error
    except FileNotFoundError as error:
        raise RuntimeError(
            f'{description} could not start {command[0]}: {error}'
        ) from error
    except OSError as error:
        raise RuntimeError(f'{description} failed to run: {error}') from error


def _parse_rpm_output(output, root):
    versions = {}
    for line_number, line in enumerate(output.splitlines(), 1):
        if line == '':
            raise RuntimeError(
                f'rpm returned a blank line at line {line_number}'
            )
        fields = line.split(chr(9))
        if len(fields) != 2:
            raise RuntimeError(
                f'rpm returned malformed output at line {line_number}: {line!r}'
            )
        name, version = fields
        if not name.strip() or not version.strip():
            raise RuntimeError(
                f'rpm returned an empty name or version at line {line_number}'
            )
        if name not in CORE_PACKAGES:
            raise RuntimeError(
                f'rpm returned unexpected package {name!r}; expected only the fixed Plasma core set'
            )
        if name in versions:
            raise RuntimeError(f'rpm returned {name!r} more than once')
        versions[name] = version

    missing = [name for name in CORE_PACKAGES if name not in versions]
    if missing:
        missing_text = ', '.join(missing)
        raise RuntimeError(
            f'missing Plasma package(s) for root {root}: {missing_text}'
        )

    baseline = versions['kglobalacceld']
    mismatched = [
        f'{name}={versions[name]}'
        for name in CORE_PACKAGES
        if versions[name] != baseline
    ]
    if mismatched:
        mismatched_text = ', '.join(mismatched)
        raise RuntimeError(
            f'Plasma package versions disagree with kglobalacceld {baseline}: {mismatched_text}'
        )
    return versions


def verify_packages(root=Path('/')):
    root = Path(root)
    command = [
        'rpm',
        '--root',
        str(root),
        '--dbpath',
        RPM_DBPATH,
        '-q',
        '--qf',
        RPM_QUERY_FORMAT,
        *CORE_PACKAGES,
    ]
    completed = _run(command, 'rpm package query', RPM_TIMEOUT_SECONDS)
    stdout = completed.stdout or ''
    stderr = (completed.stderr or '').strip()
    if completed.returncode != 0:
        try:
            _parse_rpm_output(stdout, root)
        except RuntimeError as error:
            detail = f': {stderr}' if stderr else ''
            raise RuntimeError(f'{error}{detail}') from error
        detail = f': {stderr}' if stderr else ''
        raise RuntimeError(
            f'rpm package query failed with exit status {completed.returncode}{detail}'
        )
    return _parse_rpm_output(stdout, root)


def _combined_output(result):
    return chr(10).join(
        part for part in (result.stdout, result.stderr) if part
    )


def verify_runtime(root=Path('/')):
    if Path(root).resolve() != Path('/'):
        raise RuntimeError('runtime verification requires --root /')

    ldd = _run(
        ['ldd', '-r', KWIN_BINARY],
        'ldd runtime check',
        LDD_TIMEOUT_SECONDS,
    )
    ldd_output = _combined_output(ldd)
    if ldd.returncode != 0:
        raise RuntimeError(
            f'ldd -r {KWIN_BINARY} exited {ldd.returncode}: {ldd_output.strip()}'
        )
    lowered = ldd_output.lower()
    if 'undefined symbol' in lowered or 'not found' in lowered:
        marker = 'undefined symbol' if 'undefined symbol' in lowered else 'not found'
        raise RuntimeError(
            f'KWin runtime reports {marker} from ldd -r: {ldd_output.strip()}'
        )

    with tempfile.TemporaryDirectory(prefix='kwin-version-') as directory:
        kwin_copy = Path(directory) / 'kwin_wayland'
        try:
            shutil.copyfile(KWIN_BINARY, kwin_copy)
            kwin_copy.chmod(0o755)
        except OSError as error:
            raise RuntimeError(
                f'KWin version check could not prepare a private copy of {KWIN_BINARY}: {error}'
            ) from error
        kwin = _run(
            [str(kwin_copy), '--version'],
            'KWin version check',
            VERSION_TIMEOUT_SECONDS,
        )
    if kwin.returncode != 0:
        raise RuntimeError(
            f'KWin version check exited {kwin.returncode}: {_combined_output(kwin).strip()}'
        )

    plasmashell = _run(
        ['/usr/bin/plasmashell', '--version'],
        'plasmashell version check',
        VERSION_TIMEOUT_SECONDS,
    )
    if plasmashell.returncode != 0:
        raise RuntimeError(
            f'plasmashell version check exited {plasmashell.returncode}: {_combined_output(plasmashell).strip()}'
        )


def main(argv=None):
    parser = argparse.ArgumentParser(
        description='Verify that installed Plasma packages and KWin agree.'
    )
    parser.add_argument(
        '--root',
        default='/',
        help='Filesystem root containing the RPM database (default: /).',
    )
    parser.add_argument(
        '--check-runtime',
        action='store_true',
        help='Run the KWin runtime checks; only valid with --root /.',
    )
    args = parser.parse_args(argv)
    root = Path(args.root)
    if args.check_runtime and root.resolve() != Path('/'):
        parser.error('--check-runtime is only valid with --root /')

    try:
        verify_packages(root)
        if args.check_runtime:
            verify_runtime(root)
    except RuntimeError as error:
        print(f'plasma verification failed: {error}', file=sys.stderr)
        return 1

    if args.check_runtime:
        print('Plasma package and runtime verification passed')
    else:
        print('Plasma package verification passed')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())