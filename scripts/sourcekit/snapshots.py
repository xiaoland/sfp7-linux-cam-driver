# SPDX-License-Identifier: MIT
"""Compare and replace the generated view, including paths, modes and symlinks."""

import hashlib
import json
import os
from pathlib import Path
import shutil

from .inputs import SourceError
from .trees import git


def file_records(directory):
    records = {}
    for path in sorted(directory.rglob('*')):
        if path.is_symlink():
            mode, content = '120000', os.fsencode(os.readlink(path))
        elif path.is_file():
            mode = '100755' if path.stat().st_mode & 0o111 else '100644'
            content = path.read_bytes()
        elif path.is_dir():
            continue
        else:
            raise SourceError(f'unsupported snapshot entry: {path}')
        records[path.relative_to(directory).as_posix()] = {
            'mode': mode, 'sha256': hashlib.sha256(content).hexdigest()}
    return records


def compare(expected, actual):
    missing = sorted(expected.keys() - actual.keys())
    extra = sorted(actual.keys() - expected.keys())
    drift = sorted(path for path in expected.keys() & actual.keys()
                   if expected[path] != actual[path])
    if missing or extra or drift:
        raise SourceError(f'snapshot mismatch: missing={missing}, extra={extra}, drift={drift}')


def publish(root, generated, destination, identity):
    # Refuse to overwrite local edits, including ignored/untracked snapshot files.
    relative = destination.relative_to(root).as_posix()
    dirty = git(root, 'status', '--porcelain', '--untracked-files=all',
                '--ignored', '--', relative)
    if dirty:
        raise SourceError(f'snapshot contains local edits; commit or preserve them first: {relative}')
    manifest = root / 'source' / f'{identity["component"]}.manifest.json'
    if git(root, 'status', '--porcelain', '--', str(manifest)):
        raise SourceError(f'manifest contains local edits: {manifest.name}')
    pending_manifest = generated.parent / 'manifest.json'
    pending_manifest.write_text(json.dumps(identity, indent=2, sort_keys=True) + '\n')
    backup = generated.parent / 'previous-snapshot'
    if destination.exists():
        destination.rename(backup)
    try:
        generated.rename(destination)
        pending_manifest.replace(manifest)
    except BaseException:
        if destination.exists():
            shutil.rmtree(destination)
        if backup.exists():
            backup.rename(destination)
        raise
