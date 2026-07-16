"""
PlatformIO pre-build script: inject a traceable build ID and, for the default
environment, the configured release version.

The user-facing version remains short (for example, v0.2.0).  The commit SHA
and dirty-worktree marker are available separately as CROSSPOINT_BUILD_ID for
serial logs.
"""

import configparser
import hashlib
import os
import subprocess
import sys


def warn(msg):
    print(f'WARNING [git_branch.py]: {msg}', file=sys.stderr)


def run_git_value(project_dir, args, label):
    try:
        value = subprocess.check_output(
            ['git', *args],
            text=True, stderr=subprocess.PIPE, cwd=project_dir
        ).strip()
        # Strip characters that would break a C string literal
        return ''.join(c for c in value if c not in '"\\')
    except FileNotFoundError:
        warn(f'git not found on PATH; {label} suffix will be "unknown"')
        return 'unknown'
    except subprocess.CalledProcessError as e:
        warn(
            f'git command failed (exit {e.returncode}): '
            f'{e.stderr.strip()}; {label} suffix will be "unknown"'
        )
        return 'unknown'
    except OSError as e:
        warn(
            f'OS error reading git {label}: {e}; '
            f'{label} suffix will be "unknown"'
        )
        return 'unknown'
    except Exception as e:  # pylint: disable=broad-exception-caught
        warn(
            f'Unexpected error reading git {label}: {e}; '
            f'{label} suffix will be "unknown"'
        )
        return 'unknown'


def get_git_short_sha(project_dir):
    return run_git_value(
        project_dir, ['rev-parse', '--short', 'HEAD'], 'short SHA'
    )


def get_worktree_suffix(project_dir):
    """Return a stable suffix for tracked, uncommitted source changes."""
    try:
        status = subprocess.check_output(
            ['git', 'status', '--porcelain', '--untracked-files=no'], cwd=project_dir
        )
        if not status.strip():
            return ''
        diff = subprocess.check_output(
            ['git', 'diff', '--binary', 'HEAD'], cwd=project_dir
        )
        return f'-dirty-{hashlib.sha256(diff).hexdigest()[:8]}'
    except (FileNotFoundError, subprocess.CalledProcessError, OSError) as e:
        warn(f'could not fingerprint working tree: {e}')
        return '-dirty'


def get_base_version(project_dir):
    """Read the release version from platformio.ini."""
    ini_path = os.path.join(project_dir, 'platformio.ini')
    if not os.path.isfile(ini_path):
        warn(f'platformio.ini not found at {ini_path}; base version will be "0.0.0"')
        return '0.0.0'
    config = configparser.ConfigParser()
    config.read(ini_path)
    if not config.has_option('crosspoint', 'version'):
        warn('No [crosspoint] version in platformio.ini; base version will be "0.0.0"')
        return '0.0.0'
    return config.get('crosspoint', 'version')


def inject_version(env):
    project_dir = env['PROJECT_DIR']
    short_sha = get_git_short_sha(project_dir)
    build_id = f'{short_sha}{get_worktree_suffix(project_dir)}'
    env.Append(CPPDEFINES=[('CROSSPOINT_BUILD_ID', f'\\"{build_id}\\"')])

    if env['PIOENV'] != 'default':
        return

    version = get_base_version(project_dir)
    env.Append(CPPDEFINES=[('CROSSPOINT_VERSION', f'\\"{version}\\"')])
    print(f'CrossPoint build version: {version} (build {build_id})')


# PlatformIO/SCons entry point — Import and env are SCons builtins injected at runtime.
# When run directly with Python (e.g. for validation), a lightweight fake env is used
# so the git/version logic can be exercised without a full build.
try:
    Import('env')           # noqa: F821  # type: ignore[name-defined]
    inject_version(env)     # noqa: F821  # type: ignore[name-defined]
except NameError:
    class _Env(dict):
        def Append(self, **_): pass

    _project_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    inject_version(_Env({'PIOENV': 'default', 'PROJECT_DIR': _project_dir}))
