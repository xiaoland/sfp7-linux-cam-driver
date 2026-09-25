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
records the reconstructed Git tree, each selected file's identity and the hashes
of the generator entrypoint/modules. Generator changes also require regeneration.

`prepare` requires a nonexistent output directory and leaves a clean Git checkout
plus `.sfp7-source.json`. Failure discards only its newly created temporary tree;
it does not reset an existing developer checkout. Patch failures name the exact
layer and file. Fix the input, then retry with a new output directory.
The complete kernel tree requires a case-sensitive filesystem. On a
case-insensitive volume, distinct upstream names can collide; `prepare` now
rejects a checkout whose files differ from the pinned Git tree. Use a Linux
filesystem for builds or export the verified Git tree with `git archive` to one.

`snapshot` independently replays the pinned inputs, rather than trusting a local
prepared tree. It refuses to replace local edits in the browsing directory or its
manifest: commit or preserve those edits first. This includes content/mode changes
hidden by Git's assume-unchanged, skip-worktree or filemode settings. Git subprocesses
discard inherited `GIT_*` variables so an external repository/index cannot redirect
replay or cleanup. `check` independently rebuilds and
rejects missing, extra, changed or differently typed/mode files, as well as a stale
manifest. Traditional patches, renames, deletions and paths changed then restored
are handled by Git tree transitions. Source equality is not a hardware test.

## Components and build profiles

The same commands accept `--component kernel` and `--component snapshot`.
Kernel replay uses the four original layers and checks all four intermediate tree
identities, then adds the maintenance series for `runtime-browse`. The `p1-runtime`
profile ends at the original tested tree. Its explicit `sources/kernel.paths` selection preserves the 793-file
browsing scope; a locally changed runtime file omitted from the selector is an
error. `--repository` can supply an existing local Git mirror to avoid downloading
the base again. Snapshot uses the pinned GNOME release archive; its `runtime`
profile keeps the original patch, while `runtime-browse` adds the maintenance comment.
The zero-bitrate explanation follows the [GStreamer VP8 encoder contract](https://gstreamer.freedesktop.org/documentation/vpx/GstVPXEnc.html#GstVPXEnc:target-bitrate).

For Linux libcamera development, use `--profile fedora42`. This applies the three
unaltered Fedora compiler patches before the runtime series. Fedora's third patch
contains an orphan `diff --git`/`index` header pair: replay drops only that empty
section in memory when the input group explicitly enables this compatibility rule.
The recorded original patch bytes and hashes remain unchanged. The browsing
profile omits Fedora compiler patches and must not be described as the complete
Fedora build tree. Dist-git identity and original spec digest are in the input file.

The historical RPM overlays are digest checked and their added `PatchN` order is
compared to the runtime series. This checks source declarations; it does not build
RPMs or claim that future maintenance patches are already integrated into those
historical package recipes. Candidate builds must use a new identity and be
validated before installation. No binary or device acceptance is inherited.

## Runtime configuration boundary

The P1 loader keeps each module/path/digest together; tests lock those records to
the original P1 identities and check the service's matching kernel release. Its
explicit load sequence retains the MMU pin step before ISYS. These records are
not regenerated from newly compiled modules and do not authorize a candidate
kernel installation.

The WirePlumber snippet disables the V4L2 monitor for the entire `main` profile,
including any unrelated V4L2 devices using that profile. It is not a filter for
only the internal SP7 cameras. Changing that scope requires separate device tests.
The service's Documentation path assumes a future installer places the README
there; the source collection does not currently own those installed files.
