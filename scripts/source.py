#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Reconstruct pinned source inputs; generate or verify the source browsing view."""

import argparse
import json
from pathlib import Path
import sys
import tempfile

from sourcekit.inputs import SourceError, load_inputs
from sourcekit.snapshots import compare, file_records, publish
from sourcekit.trees import materialize, prepare, replay, tree_id


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['prepare', 'snapshot', 'check'])
    parser.add_argument('--component', required=True, choices=['libcamera'])
    parser.add_argument('--profile', default='runtime-browse')
    parser.add_argument('--archive', type=Path, help='verified local upstream archive')
    parser.add_argument('--repository', type=Path, help='local Git repository containing the pinned base')
    parser.add_argument('--cache', type=Path, default=Path.home() / '.cache/sfp7-camera/sources')
    parser.add_argument('--output', type=Path, help='new complete source directory (prepare only)')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if sys.version_info < (3, 12):
        parser.error('Python 3.12 or newer is required for safe archive extraction')
    if args.action == 'prepare':
        if args.output is None or args.output.exists() or args.output.is_symlink():
            parser.error('prepare requires a new, nonexistent --output directory')
        args.output = args.output.absolute()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        parent = args.output.parent
    else:
        if args.output:
            parser.error('--output is only valid for prepare')
        parent = root / 'source'
    data, groups, fingerprint = load_inputs(root, args.component, args.profile)
    if args.action != 'prepare' and args.profile != data['snapshot_profile']:
        raise SourceError('only the declared browsing profile may generate/check the snapshot')
    with tempfile.TemporaryDirectory(prefix='.sfp7-replay-', dir=parent) as temporary:
        scratch = Path(temporary)
        tree = scratch / 'tree'
        entries = replay(tree, root, data, groups, args.archive, args.cache, args.repository)
        identity = {'schema': 1, 'component': args.component, 'profile': args.profile,
                    'inputs_sha256': fingerprint, 'tree': tree_id(tree)}
        if args.action == 'prepare':
            prepare(tree, args.output, identity)
        else:
            generated = scratch / 'snapshot'
            materialize(tree, entries, generated)
            identity['files'] = file_records(generated)
            destination = root / data['snapshot_directory']
            if args.action == 'snapshot':
                publish(root, generated, destination, identity)
            else:
                compare(identity['files'], file_records(destination))
                manifest = root / 'source' / f'{args.component}.manifest.json'
                if not manifest.exists() or json.loads(manifest.read_text()) != identity:
                    raise SourceError('snapshot provenance manifest differs from replay')
        print(f'{args.action}: {args.component}/{args.profile} tree={identity["tree"]}, selected={len(entries)}')


if __name__ == '__main__':
    try:
        main()
    except (SourceError, OSError, ValueError) as error:
        print(f'error: {error}', file=sys.stderr)
        sys.exit(1)
