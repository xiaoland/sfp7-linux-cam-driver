# Contributing

The immediate aim is a reviewable, reproducible Surface Pro 7 camera stack and a safe opt-in test package. Please open an issue with your Surface model, IPU PCI ID, distribution and kernel version before trying to extrapolate to another device. Surface Pro 7+ is a different platform.

For camera test reports, state the exact kernel/libcamera/Snapshot versions, Secure Boot state, whether both cameras capture, front/rear switching, repeated application opens, suspend/resume result, and any CSI/ISYS errors. Do not post photos or raw logs containing personal information without checking them first. A second machine's negative result is as useful as a positive result.

Proposed code changes should be small and attributable. Keep original upstream patch authors and license notices. The flattened kernel runtime diff is for reproducibility and must be split before an upstream proposal. libcamera and GNOME Snapshot patches belong in their respective projects after rebasing and review; this repository is an integration staging area.

The Linux kernel and libcamera require valid Developer Certificate of Origin sign-offs for submissions. A sign-off is a personal certification, not a formatting step. Some internal development patches had placeholder identities; the public copies intentionally omit invalid sign-offs. Do not add someone else's sign-off or claim an AI tool signed. Disclose material AI assistance and be prepared to explain and maintain the submitted code.

Relevant upstream guidance: [Linux kernel patch submission](https://docs.kernel.org/process/submitting-patches.html), [Linux kernel generated content](https://docs.kernel.org/process/generated-content.html), and [libcamera contributing](https://libcamera.org/contributing.html).
