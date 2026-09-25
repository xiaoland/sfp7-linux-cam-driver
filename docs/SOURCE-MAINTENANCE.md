# Maintaining the source views

Pinned archives, ordered patches and configuration are the editable inputs.
`source/` is generated for reading; build and edit a complete tree, export the
change as a new patch, then replay and regenerate. Preserve the historical runtime
series and its attribution. Python 3.12+ and Git are required for the generator.

```sh
python3 scripts/source.py prepare --component libcamera --output ../libcamera-work
python3 scripts/source.py snapshot --component libcamera
python3 scripts/source.py check --component libcamera
python3 -m unittest discover -s tests/source -v
```

`--archive /path/to/libcamera-0.4.0.tar.xz` uses an already downloaded archive,
with the same mandatory digest check. Downloads use the Fedora lookaside archive
and a content-addressed cache (`--cache DIR`). Inputs and license selection are
recorded in `sources/libcamera.json`; `series` controls patch order and the digest
map must contain exactly those entries. The per-component generated manifest
records the reconstructed Git tree and each selected file's identity.

`prepare` requires a nonexistent output directory and leaves a clean Git checkout
plus `.sfp7-source.json`. Failure discards only its newly created temporary tree;
it does not reset an existing developer checkout. Patch failures name the exact
layer and file. Fix the input, then retry with a new output directory.

`snapshot` independently replays the pinned inputs, rather than trusting a local
prepared tree. It refuses to replace local edits in the browsing directory or its
manifest: commit or preserve those edits first. `check` independently rebuilds and
rejects missing, extra, changed or differently typed/mode files, as well as a stale
manifest. Traditional patches, renames, deletions and paths changed then restored
are handled by Git tree transitions. Source equality is not a hardware test.
