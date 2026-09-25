# SPDX-License-Identifier: MIT
"""The comparison report must identify the files that will actually be built."""

import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[2] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
from sourcekit.trees import git, record_commit, tree_id

spec = importlib.util.spec_from_file_location('native_comparison', SCRIPTS / 'test-simple-ipa.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class NativeIdentityTest(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.actual = self.repository('actual')
        self.foreign = self.repository('foreign')

    def repository(self, name):
        root = self.root / name
        root.mkdir()
        git(root, 'init', '--quiet')
        for path in [*runner.TEST_FILES, 'production.cpp']:
            file = root / path
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text(name + '\n')
        git(root, 'add', '--all')
        record_commit(root, 'source')
        return root

    def test_foreign_git_environment_does_not_change_identity(self):
        env = {'GIT_DIR': str(self.foreign / '.git'), 'GIT_WORK_TREE': str(self.foreign)}
        with patch.dict(os.environ, env):
            self.assertEqual(runner.source_identity(self.actual)['tree'], tree_id(self.actual))
            (self.actual / 'production.cpp').write_text('uncommitted actual code')
            with self.assertRaisesRegex(ValueError, 'commit/export'):
                runner.source_identity(self.actual)

    def test_hidden_production_edits_are_rejected(self):
        target = self.actual / 'production.cpp'
        for flag in ('assume-unchanged', 'skip-worktree'):
            with self.subTest(flag=flag):
                git(self.actual, 'update-index', '--' + flag, str(target))
                target.write_text('hidden production edit')
                self.assertEqual(git(self.actual, 'status', '--porcelain'), b'')
                with self.assertRaisesRegex(ValueError, 'flags'):
                    runner.source_identity(self.actual)
                target.write_text('actual\n')
                git(self.actual, 'update-index', '--no-' + flag, str(target))
