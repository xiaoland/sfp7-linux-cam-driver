# SPDX-License-Identifier: MIT
"""Exercise source-format and failure contracts using tiny real Git trees."""

import hashlib
import io
import json
import os
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
from sourcekit.inputs import SourceError, load_inputs, relative_path
from sourcekit.snapshots import compare, file_records, publish
from sourcekit.trees import (git, import_base, inventory, materialize, prepare,
                             record_commit, replay, tree_id)


class SourceReplayTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.archive = self.root / 'base.tar.xz'
        with tarfile.open(self.archive, 'w:xz') as archive:
            for name, content in [('keep', b'original\n'), ('delete', b'obsolete\n')]:
                entry = tarfile.TarInfo('base/' + name)
                entry.size = len(content)
                entry.mode = 0o644
                archive.addfile(entry, io.BytesIO(content))
        self.base = {'kind': 'archive', 'directory': 'base', 'url': 'unused',
                     'sha256': hashlib.sha256(self.archive.read_bytes()).hexdigest()}
        self.data = {'base': self.base, 'licenses': []}

    def run_replay(self, groups=(), data=None):
        tree = self.root / 'replayed'
        selected = replay(tree, self.root, data or self.data, groups,
                          self.archive, self.root / 'cache', None)
        return tree, selected

    def patch(self, name, content):
        (self.root / name).write_bytes(content)
        return name

    def test_traditional_patch_includes_agc_style_paths(self):
        patch = self.patch('traditional.patch', b'--- a/keep\n+++ b/keep\n@@ -1 +1 @@\n-original\n+modified\n')
        tree, selected = self.run_replay([('runtime', {}, [patch])])
        self.assertEqual(set(selected), {'keep'})
        output = self.root / 'snapshot'
        materialize(tree, selected, output)
        self.assertEqual((output / 'keep').read_text(), 'modified\n')

    def test_add_delete_rename_mode_symlink_and_reverted_path(self):
        author = self.root / 'author'
        import_base(author, self.base, self.archive, self.root / 'cache', None)
        (author / 'keep').write_text('intermediate\n')
        git(author, 'add', '--all')
        first = self.patch('first.patch', git(author, 'diff', '--cached', '--binary'))
        record_commit(author, 'intermediate')
        (author / 'keep').write_text('original\n')
        (author / 'delete').rename(author / 'renamed file')
        (author / 'run').write_text('#!/bin/sh\nexit 0\n')
        (author / 'run').chmod(0o755)
        (author / 'link').symlink_to('keep')
        git(author, 'add', '--all')
        second = self.patch('second.patch', git(author, 'diff', '--cached', '--binary'))
        tree, selected = self.run_replay([('runtime', {}, [first, second])])
        self.assertEqual(set(selected), {'keep', 'renamed file', 'run', 'link'})
        output = self.root / 'snapshot'
        materialize(tree, selected, output)
        self.assertEqual(file_records(output)['run']['mode'], '100755')
        self.assertTrue((output / 'link').is_symlink())
        self.assertEqual(os.readlink(output / 'link'), 'keep')
        self.assertEqual((output / 'keep').read_text(), 'original\n')

    def test_fedora_orphan_headers_are_normalized_only_when_declared(self):
        payload = (b'diff --git a/unused b/unused\nindex abcdef0..fedcba0 100644\n'
                   b'diff --git a/keep b/keep\n--- a/keep\n+++ b/keep\n'
                   b'@@ -1 +1 @@\n-original\n+modified\n')
        patch = self.patch('fedora.patch', payload)
        tree, selected = self.run_replay([
            ('fedora', {'ignore_empty_git_headers': True}, [patch])])
        self.assertEqual(set(selected), {'keep'})
        self.assertEqual((self.root / patch).read_bytes(), payload)

    def test_missing_extra_content_and_mode_drift_are_detected(self):
        expected = {'a': {'mode': '100644', 'sha256': 'correct'}}
        for actual in [{}, {**expected, 'extra': {}},
                       {'a': {'mode': '100644', 'sha256': 'changed'}},
                       {'a': {'mode': '100755', 'sha256': 'correct'}}]:
            with self.subTest(actual=actual), self.assertRaises(SourceError):
                compare(expected, actual)
        compare(expected, expected)

    def test_digest_or_patch_failure_does_not_modify_existing_tree(self):
        existing = self.root / 'developer-tree'
        existing.mkdir()
        marker = existing / 'work'
        marker.write_text('uncommitted work')
        self.archive.write_bytes(b'wrong archive')
        with self.assertRaisesRegex(SourceError, 'digest mismatch'):
            self.run_replay()
        self.assertEqual(marker.read_text(), 'uncommitted work')

    def test_failed_patch_identifies_exact_layer(self):
        patch = self.patch('bad.patch', b'--- a/missing\n+++ b/missing\n@@ -1 +1 @@\n-a\n+b\n')
        with self.assertRaisesRegex(SourceError, 'runtime: bad.patch'):
            self.run_replay([('runtime', {}, [patch])])

    def test_prepared_checkout_contains_final_content_and_is_clean(self):
        patch = self.patch('remove.patch', b'--- a/delete\n+++ /dev/null\n@@ -1 +0,0 @@\n-obsolete\n')
        tree, selected = self.run_replay([('runtime', {}, [patch])])
        output = self.root / 'prepared'
        prepare(tree, output, {'tree': tree_id(tree)})
        self.assertFalse((output / 'delete').exists())
        self.assertEqual(git(output, 'status', '--porcelain'), b'')
        self.assertEqual(set(inventory(output)), {'keep'})

    def test_archive_escape_is_rejected(self):
        with tarfile.open(self.archive, 'w:xz') as archive:
            entry = tarfile.TarInfo('../outside')
            entry.size = 1
            archive.addfile(entry, io.BytesIO(b'x'))
        self.base['sha256'] = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        with self.assertRaises((tarfile.FilterError, SourceError)):
            self.run_replay()
        self.assertFalse((self.root / 'outside').exists())

    def test_series_and_hash_inventory_must_agree(self):
        sources = self.root / 'sources'
        sources.mkdir()
        (self.root / 'series').write_text('absent.patch\n')
        (sources / 'test.json').write_text(json.dumps({
            'schema': 1, 'profiles': {'browse': ['runtime']},
            'groups': {'runtime': {'series': 'series', 'sha256': {}}}}))
        with self.assertRaisesRegex(SourceError, 'inventory'):
            load_inputs(self.root, 'test', 'browse')

    def test_snapshot_refuses_uncommitted_changes(self):
        repo = self.root / 'repo'
        repo.mkdir()
        git(repo, 'init', '--quiet')
        destination = repo / 'source/test'
        destination.mkdir(parents=True)
        (destination / 'code').write_text('committed')
        git(repo, 'add', '--all')
        record_commit(repo, 'initial')
        (destination / 'code').write_text('user edit')
        generated = self.root / 'generated'
        generated.mkdir()
        (generated / 'code').write_text('replacement')
        with self.assertRaisesRegex(SourceError, 'local edits'):
            publish(repo, generated, destination, {'component': 'test'})
        self.assertEqual((destination / 'code').read_text(), 'user edit')

    def test_repository_paths_do_not_escape(self):
        for path in ['../outside', '/absolute', 'a/../../outside', '.git/config', '']:
            with self.subTest(path=path), self.assertRaises(SourceError):
                relative_path(path)


if __name__ == '__main__':
    unittest.main()
