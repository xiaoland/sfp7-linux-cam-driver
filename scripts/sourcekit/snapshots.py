# SPDX-License-Identifier: MIT
"""Compare and replace the generated view, including paths, modes and symlinks."""

import hashlib
import json
import os
from pathlib import Path
import shutil

from .inputs import SourceError
from .trees import git


def file_record(path):
    if path.is_symlink():
        mode, content = '120000', os.fsencode(os.readlink(path))
    elif path.is_file():
        mode = '100755' if path.stat().st_mode & 0o111 else '100644'
        content = path.read_bytes()
    else:
        raise SourceError(f'unsupported snapshot entry: {path}')
    return {'mode': mode, 'sha256': hashlib.sha256(content).hexdigest()}


def file_records(directory):
    records = {}
    for path in sorted(directory.rglob('*')):
        if path.is_dir() and not path.is_symlink():
            continue
        records[path.relative_to(directory).as_posix()] = file_record(path)
    return records


def compare(expected, actual):
    missing = sorted(expected.keys() - actual.keys())
    extra = sorted(actual.keys() - expected.keys())
    drift = sorted(path for path in expected.keys() & actual.keys()
                   if expected[path] != actual[path])
    if missing or extra or drift:
        raise SourceError(f'snapshot mismatch: missing={missing}, extra={extra}, drift={drift}')


def require_clean(root, relative):
    """Check index and actual bytes; status alone can hide worktree edits."""
    dirty = git(root, 'status', '--porcelain', '--untracked-files=all',
                '--ignored', '--', relative)
    if dirty:
        raise SourceError(f'snapshot contains local edits; commit or preserve them first: {relative}')
    expected = {}
    for item in git(root, 'ls-files', '--stage', '-z', '--', relative).split(b'\0'):
        if not item:
            continue
        metadata, path = item.split(b'\t', 1)
        mode, oid, stage = metadata.decode().split()
        if stage != '0':
            raise SourceError(f'unmerged snapshot index entry: {path!r}')
        expected[path.decode()] = {'mode': mode,
                                  'sha256': hashlib.sha256(git(root, 'cat-file', 'blob', oid)).hexdigest()}
    target = root / relative
    if target.is_dir() and not target.is_symlink():
        actual = {f'{relative}/{name}': record for name, record in file_records(target).items()}
    else:
        actual = {relative: file_record(target)} if target.exists() or target.is_symlink() else {}
    try:
        compare(expected, actual)
    except SourceError as error:
        raise SourceError(f'snapshot contains local edits hidden from Git status: {relative}: {error}') from error


def publish(root, generated, destination, identity):
    # Refuse to overwrite local edits, including ignored/untracked snapshot files.
    relative = destination.relative_to(root).as_posix()
    require_clean(root, relative)
    manifest = root / 'source' / f'{identity["component"]}.manifest.json'
    require_clean(root, manifest.relative_to(root).as_posix())
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
