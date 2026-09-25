# Contributing

The immediate aim is a reviewable, reproducible Surface Pro 7 camera stack and a safe opt-in test package. Please open an issue with your Surface model, IPU PCI ID, distribution and kernel version before trying to extrapolate to another device. Surface Pro 7+ is a different platform.

For camera test reports, state the exact kernel/libcamera/Snapshot versions, Secure Boot state, whether both cameras capture, front/rear switching, repeated application opens, suspend/resume result, and any CSI/ISYS errors. Do not post photos or raw logs containing personal information without checking them first. A second machine's negative result is as useful as a positive result.

Proposed code changes should be small and attributable. Keep original upstream patch authors and license notices. The flattened kernel runtime diff is for reproducibility and must be split before an upstream proposal. libcamera and GNOME Snapshot patches belong in their respective projects after rebasing and review; this repository is an integration staging area.

The Linux kernel and libcamera require valid Developer Certificate of Origin sign-offs for submissions. A sign-off is a personal certification, not a formatting step. Some internal development patches had placeholder identities; the public copies intentionally omit invalid sign-offs. Do not add someone else's sign-off or claim an AI tool signed. Disclose material AI assistance and be prepared to explain and maintain the submitted code.

Relevant upstream guidance: [Linux kernel patch submission](https://docs.kernel.org/process/submitting-patches.html), [Linux kernel generated content](https://docs.kernel.org/process/generated-content.html), and [libcamera contributing](https://libcamera.org/contributing.html).

## Source changes

Start with the [source maintenance guide](docs/SOURCE-MAINTENANCE.md) and
[test contracts](tests/README.md). Prepare a complete source tree, edit it there,
then export one logical patch per change. Add its path to the appropriate series
and its SHA-256 to the input manifest. Regenerate and check the browsing snapshot
from the updated pinned inputs; do not maintain a second hand-edited source copy.

For readability work, describe the responsibility or naming problem and the
behavior that must stay fixed. Keep naming/comments, helper extraction and
functional changes in separate commits. For AF/AGC, compare the identical native
test input against the unrefactored baseline before changing expected behavior.
For kernel work, keep the original lock/MMIO/cleanup order visible and run the
relevant source models plus a full build. Hardware evidence must name the actual
candidate version. A shorter file or a passing host model alone is insufficient.
