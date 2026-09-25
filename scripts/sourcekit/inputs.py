# SPDX-License-Identifier: MIT
"""Validate repository inputs before opening or modifying an output tree."""

import hashlib
import json
import re
from pathlib import Path, PurePosixPath
import urllib.request


class SourceError(Exception):
    """An input or generated source does not match its declared identity."""


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def relative_path(value):
    path = PurePosixPath(value)
    if not value or path.is_absolute() or '..' in path.parts or '.git' in path.parts:
        raise SourceError(f'unsafe repository path: {value!r}')
    return path


def read_series(root, name):
    file = root / relative_path(name)
    entries = []
    for line in file.read_text().splitlines():
        entry = line.split('#', 1)[0].strip()
        if entry:
            relative_path(entry)
            entries.append((file.parent / entry).relative_to(root).as_posix())
    if len(entries) != len(set(entries)):
        raise SourceError(f'duplicate patch in {name}')
    return entries


def load_inputs(root, component, profile):
    file = root / 'sources' / f'{component}.json'
    data = json.loads(file.read_text())
    if data['schema'] != 1 or profile not in data['profiles']:
        raise SourceError(f'unsupported schema/profile: {component}/{profile}')
    fingerprint = hashlib.sha256(file.read_bytes())
    groups = []
    for name in data['profiles'][profile]:
        group = data['groups'][name]
        entries = read_series(root, group['series'])
        if set(entries) != set(group['sha256']):
            raise SourceError(f'patch inventory differs from hashes: {name}')
        fingerprint.update((root / group['series']).read_bytes())
        for entry in entries:
            if digest(root / entry) != group['sha256'][entry]:
                raise SourceError(f'patch digest mismatch: {entry}')
        groups.append((name, group, entries))
    for entry, expected in data.get('auxiliary_sha256', {}).items():
        if digest(root / relative_path(entry)) != expected:
            raise SourceError(f'auxiliary input digest mismatch: {entry}')
    for binding in data.get('spec_patch_series', []):
        overlay = (root / relative_path(binding['spec_overlay'])).read_text()
        declared = re.findall(r'^\+Patch[0-9]*:\s*(\S+)', overlay, re.MULTILINE)
        group = data['groups'][binding['group']]
        ordered = [Path(path).name for path in read_series(root, group['series'])]
        if declared != ordered:
            raise SourceError(f"spec patch order differs from series: {binding['spec_overlay']}")
    return data, groups, fingerprint.hexdigest()


def source_archive(base, supplied, cache):
    if supplied:
        path = supplied.resolve()
    else:
        cache.mkdir(parents=True, exist_ok=True)
        path = cache / base['sha256']
        if not path.exists():
            import tempfile
            with tempfile.NamedTemporaryFile(dir=cache, delete=False) as stream:
                temporary = Path(stream.name)
                try:
                    with urllib.request.urlopen(base['url'], timeout=60) as response:
                        import shutil
                        shutil.copyfileobj(response, stream)
                    stream.flush()
                    if digest(temporary) != base['sha256']:
                        raise SourceError('downloaded archive digest mismatch')
                    temporary.replace(path)
                finally:
                    temporary.unlink(missing_ok=True)
    if digest(path) != base['sha256']:
        raise SourceError(f'archive digest mismatch: {path}')
    return path
