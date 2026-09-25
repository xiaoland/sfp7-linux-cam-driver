#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build two complete Linux source trees and compare real Simple IPA requests."""

import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import subprocess
import sys

OPTIONS = [
    '--wrap-mode=nofallback', '-Dpipelines=simple', '-Dipas=simple', '-Dtest=true',
    '-Dcam=disabled', '-Dqcam=disabled', '-Dgstreamer=disabled',
    '-Dlc-compliance=disabled', '-Dpycamera=disabled', '-Ddocumentation=disabled',
    '-Dv4l2=false', '-Dtracing=disabled', '-Db_sanitize=address,undefined',
]
TEST_FILES = ['test/ipa/simple/' + name for name in
              ['af.cpp', 'agc.cpp', 'test_support.h', 'meson.build']]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_identity(source):
    dirty = subprocess.check_output(['git', '-C', str(source), 'status', '--porcelain'])
    if dirty:
        raise ValueError(f'commit/export source edits before comparing: {source}')
    tree = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD^{tree}'], text=True).strip()
    return {'tree': tree, 'tests': {path: sha(source / path) for path in TEST_FILES}}


def run(command, output, label, commands, env=None):
    commands.append(command)
    with (output / f'{label}.stdout').open('w') as stdout, (output / f'{label}.stderr').open('w') as stderr:
        subprocess.run(command, stdout=stdout, stderr=stderr, check=True, env=env)


def compare_trace(baseline, candidate):
    with baseline.open() as before, candidate.open() as after:
        for row, (left, right) in enumerate(itertools.zip_longest(before, after), 1):
            if left != right:
                raise ValueError(f'{candidate.name}: first difference at row {row}: {left!r} -> {right!r}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True, type=Path)
    parser.add_argument('--candidate', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--jobs', type=int, default=min(os.cpu_count() or 1, 8))
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        parser.error('run in Linux; this test compiles the complete real dependency graph')
    if args.output.exists() or args.output.is_symlink() or args.jobs < 1:
        parser.error('use a new output directory and positive --jobs')
    args.output = args.output.resolve()
    sources = {'baseline': args.baseline.resolve(), 'candidate': args.candidate.resolve()}
    identities = {name: source_identity(path) for name, path in sources.items()}
    if identities['baseline']['tests'] != identities['candidate']['tests']:
        parser.error('both trees must compile identical test inputs')
    args.output.mkdir(parents=True)
    report = {'schema': 1, 'sources': identities, 'commands': [], 'passed': False,
              'hardware_tested': False, 'sanitizers': 'address,undefined'}
    try:
        for name, source in sources.items():
            build = args.output / f'build-{name}'
            run(['meson', 'setup', str(build), str(source), *OPTIONS], args.output,
                f'{name}-setup', report['commands'])
            run(['meson', 'compile', '-C', str(build), f'-j{args.jobs}'], args.output,
                f'{name}-build', report['commands'])
            run(['meson', 'test', '-C', str(build), 'simple-af', 'simple-agc', '--print-errorlogs'],
                args.output, f'{name}-tests', report['commands'])
            for algorithm in ('af', 'agc'):
                env = dict(os.environ, LIBCAMERA_LOG_LEVELS='*:ERROR')
                run([str(build / f'test/ipa/simple/simple-{algorithm}')], args.output,
                    f'{name}-{algorithm}', report['commands'], env)
        report['traces'] = {}
        for algorithm in ('af', 'agc'):
            before = args.output / f'baseline-{algorithm}.stdout'
            after = args.output / f'candidate-{algorithm}.stdout'
            compare_trace(before, after)
            report['traces'][algorithm] = {'sha256': sha(after),
                                          'callbacks': len(after.read_text().splitlines())}
        report['passed'] = True
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        report['error'] = str(error)
    finally:
        (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report.get('traces', report.get('error')), indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
