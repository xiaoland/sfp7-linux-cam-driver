# Maintenance candidate validation — 2026-09-25

The refactor makes the source composition reproducible and separates AF/AGC
responsibilities while keeping the original runtime patches and P1 identities.
The reviewed code is `23a920b6d14e5405238041e3c647e84c683960bc`.
This page records the offline development checks. The later
[single-device validation](MAINTENANCE-DEVICE-VALIDATION.md) records the
candidate installed on a Surface Pro 7; the [P1 results](STATUS.md) describe
another tree.

## Changes and retained boundaries

- `sources/` pins each component's inputs and profiles. The generator prepares a
  new complete tree and checks the browsing view's file set, bytes, modes, links
  and provenance. It also records the generator implementation hashes.
- AF names commanded lens positions and callback counts explicitly, with private
  helpers for focus loss, rescan readiness and reference monitoring. AGC separates
  startup boosting from the caller's guards, valid-callback budget and steady control.
- Simple's reopen policy, BLC estimate validity and sensor/lens/statistics units are
  documented at their boundaries. The statistics macro remains in place because
  representative RAW performance equivalence has not been established.
- Kernel changes isolate stream-input configuration and clarify verification
  rounds, message ownership and diagnostics. Register accesses, lock/atomic order,
  timeouts, control thresholds and cleanup protocol remain in their original order.
- The loader keeps each module/path/digest together. It retains the original seven
  P1 identities, explicit load sequence, MMU pin step and one-attempt rule.

Large imported files were kept where their module boundary remained appropriate.
The changes do not attempt a vendor-driver rewrite, new concurrency protocol,
image-quality tuning, packaging/signing or a wider hardware support claim.

## Checks

The accompanying [machine-readable record](validation/maintainability-20260925.json)
binds the source trees, configuration, tools and observable output hashes.

| Check | Result and scope |
| --- | --- |
| Source and native identity fixtures | 16 tests; real Git/archive transitions, failures, hidden edits, external Git environment and relative mirror paths |
| Replayed browsing views | kernel 793, libcamera 41, Snapshot 4 entries; complete set/content/type/mode and provenance equality |
| Native Simple IPA | Complete Linux baseline/candidate builds with ASan/UBSan; 38,402 AF and 2,480 AGC callback outputs identical |
| Boundary mutations | All four changes to cooldown/loss/recovery/startup budget detected by assertions or trace comparison |
| Historical AGC | 77 cases, including 60 steady comparisons; original 0017/0018 source, explicit framework substitutes |
| BLC/LUT | 12 cases with identical outputs and ASan/UBSan; complete classes/data definitions, substituted framework boundaries |
| Kernel source models | 44 cases; actual selected functions with handwritten FW/kernel dependencies, without sanitizers |
| P1 loader | 5 Linux tests, including 6 identity-failure cases; private fake filesystem and commands |
| Full kernel | `bzImage` and all 4,971 configured modules built successfully; seven camera modules have the candidate vermagic |

The Linux environment uses the Fedora 42 x86_64 image pinned in
[Containerfile](../tests/Containerfile), with GCC 15.2.1, Meson 1.7.2 and Ninja
1.12.1. Kernel compilation starts from the pinned P1 configuration and uses
`LOCALVERSION=-sfp7cam.maint.fc42.x86_64`. `olddefconfig` changes only detected
Rust-toolchain capabilities and pahole version in this environment; those exact
differences are recorded. No source configuration options were retuned. Two log-format warnings in
`drivers/platform/x86/intel/int3472/discrete.c` are inherited unchanged from the
pinned base; the build completed without errors. The build reused completed
objects, with affected sources rebuilt after the final patches.

Reproduction entry points: [source guide](SOURCE-MAINTENANCE.md),
[tests](../tests/README.md), and [kernel build guide](BUILDING.md).
Source/tree equality is not a claim of bit-for-bit binary reproducibility.

## Independent review

A separate AI reviewer inspected the changes and necessary source context after
implementation. Seven findings were fixed and independently rechecked: Git
repository redirection, hidden snapshot edits, incorrect native-test source
identity, relative mirror paths, build-example paths, misplaced kernel comments,
and incomplete loader identity-test coverage. No reported finding remains open.
This review does not replace upstream human review or DCO certification.

The reviewer ran the 16 source/identity tests and 44 driver cases, and checked all
profiles plus snapshot/manifests. Linux full builds and native/loader results
were produced separately by the implementing agent. Neither agent exercised
camera hardware during the offline refactor and review stage.

Outstanding boundaries include OPEN/CLOSE confirmation and late reply/handle
reuse, request error propagation, real control delays and lens mechanics,
IRQ/FW/PM behavior, video quality and another SP7. The subsequent single-device
hardware acceptance and remaining packaging work are tracked separately.
