# SPDX-License-Identifier: MIT
"""Replay into a disposable Git tree; never reset an existing developer checkout."""

import json
import os
import re
from pathlib import Path
import subprocess
import tarfile

from .inputs import SourceError, relative_path, source_archive


def git(tree, *args, input=None):
    # -C does not override GIT_DIR, GIT_WORK_TREE or GIT_INDEX_FILE. Clear all
    # Git-specific environment (including injected config) before any command:
    # prepare's reset/clean must only act on our disposable repository.
    env = {key: value for key, value in os.environ.items() if not key.startswith('GIT_')}
    env['GIT_CONFIG_NOSYSTEM'] = '1'
    # Isolate replay from signing, hooks and line-ending filters in global config.
    env['GIT_CONFIG_GLOBAL'] = os.devnull
    result = subprocess.run(['git', '-C', str(tree), *args], input=input,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    if result.returncode:
        raise SourceError(f'git {args[0]}: {result.stderr.decode(errors="replace").strip()}')
    return result.stdout


def tree_id(tree):
    return git(tree, 'write-tree').decode().strip()


def record_commit(tree, message):
    git(tree, '-c', 'user.name=Source replay', '-c',
        'user.email=source-replay@invalid', 'commit', '--quiet', '--allow-empty', '-m', message)


def import_base(tree, base, archive, cache, repository):
    tree.mkdir()
    git(tree, 'init', '--quiet')
    git(tree, 'config', 'core.autocrlf', 'false')
    if base['kind'] == 'archive':
        path = source_archive(base, archive, cache)
        # Extract into a separate directory so an archive cannot replace .git.
        import tempfile
        with tempfile.TemporaryDirectory(dir=tree.parent) as temporary:
            scratch = Path(temporary)
            try:
                with tarfile.open(path) as bundle:
                    bundle.extractall(scratch, filter='data')
            except tarfile.TarError as exc:
                raise SourceError(f'unsafe or invalid source archive: {exc}') from exc
            top = scratch / relative_path(base['directory'])
            if not top.is_dir() or (top / '.git').exists():
                raise SourceError('archive does not contain the declared source root')
            for entry in top.iterdir():
                entry.rename(tree / entry.name)
        git(tree, 'add', '--force', '--all')
        record_commit(tree, 'Import verified upstream source archive')
    else:
        # Fetch copies objects: prepared trees do not depend on a local alternate.
        git(tree, 'fetch', '--quiet', '--depth=1', '--no-tags',
            str(repository.resolve() if repository else base['url']), base['commit'])
        git(tree, 'reset', '--soft', 'FETCH_HEAD')
        git(tree, 'read-tree', 'HEAD')
    if base.get('tree') and tree_id(tree) != base['tree']:
        raise SourceError('upstream tree identity mismatch')


def changed_paths(tree, before, after):
    # --no-renames emits delete/add paths; -z preserves spaces and special names.
    raw = git(tree, 'diff', '--name-only', '--no-renames', '-z', before, after)
    return {path.decode() for path in raw.split(b'\0') if path}


def inventory(tree):
    result = {}
    for item in git(tree, 'ls-tree', '-rz', tree_id(tree)).split(b'\0'):
        if item:
            metadata, name = item.split(b'\t', 1)
            mode, kind, oid = metadata.decode().split()
            if kind != 'blob':
                raise SourceError(f'unsupported Git entry: {name!r}')
            result[name.decode()] = (mode, oid)
    return result


def patch_payload(path, group):
    content = path.read_bytes()
    if group.get('ignore_empty_git_headers', False):
        # Fedora's pinned GCC 15 gtest patch starts with an orphan diff/index
        # pair. GNU patch ignores it; Git rejects it. Keep the input unchanged
        # and remove only sections consisting entirely of those two headers.
        sections = re.split(rb'(?=^diff --git )', content, flags=re.MULTILINE)
        content = b''.join(section for section in sections if not re.fullmatch(
            rb'diff --git [^\n]+\nindex [^\n]+\n', section))
    return content


def replay(tree, root, data, groups, archive, cache, repository):
    import_base(tree, data['base'], archive, cache, repository)
    touched = set()
    for name, group, patches in groups:
        for patch in patches:
            before = tree_id(tree)
            try:
                payload = patch_payload(root / patch, group)
                git(tree, 'apply', '--cached', '--check', '--binary', '-', input=payload)
                git(tree, 'apply', '--cached', '--binary', '-', input=payload)
            except SourceError as exc:
                raise SourceError(f'{name}: {patch}: {exc}') from exc
            after = tree_id(tree)
            if group.get('snapshot', True):
                touched.update(changed_paths(tree, before, after))
        if group.get('tree') and tree_id(tree) != group['tree']:
            raise SourceError(f'{name}: reconstructed tree identity mismatch')
    entries = inventory(tree)
    if 'selection' in data:
        selection_file = root / relative_path(data['selection'])
        selected = set(selection_file.read_text().splitlines())
        if not touched <= selected:
            raise SourceError(f'kernel selector omits touched paths: {sorted(touched - selected)}')
    else:
        selected = touched & entries.keys()
    selected.update(data.get('licenses', []))
    for path in selected:
        relative_path(path)
        if path not in entries:
            raise SourceError(f'selected file absent from final tree: {path}')
    return {path: entries[path] for path in sorted(selected)}


def materialize(tree, entries, output):
    output.mkdir(parents=True, exist_ok=True)
    for path, (mode, oid) in entries.items():
        target = output / relative_path(path)
        target.parent.mkdir(parents=True, exist_ok=True)
        content = git(tree, 'cat-file', 'blob', oid)
        if mode == '120000':
            target.symlink_to(os.fsdecode(content))
        elif mode in ('100644', '100755'):
            target.write_bytes(content)
            target.chmod(int(mode[-3:], 8))
        else:
            raise SourceError(f'unsupported mode {mode}: {path}')


def prepare(tree, output, identity):
    record_commit(tree, 'Apply pinned camera source layers')
    # Rebuild tracked files from the index, including deletions from the archive.
    git(tree, 'reset', '--hard', '--quiet', 'HEAD')
    # Cached deletions leave old archive files untracked. This tree is our new
    # disposable replay directory, never an existing developer checkout.
    git(tree, 'clean', '-fdx', '--quiet')
    (tree / '.sfp7-source.json').write_text(json.dumps(identity, indent=2) + '\n')
    exclude = tree / '.git/info/exclude'
    with exclude.open('a') as stream:
        stream.write('\n.sfp7-source.json\n')
    tree.rename(output)
