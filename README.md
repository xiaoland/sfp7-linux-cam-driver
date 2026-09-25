# Surface Pro 7 Linux camera stack

Experimental source and reproducibility material for the **Surface Pro 7 (2019, IPU4P PCI ID `8086:8a19`)** front and rear cameras on Linux. This repository combines a pinned kernel source composition, libcamera Software ISP changes, a GNOME Snapshot video fix, and runtime configuration. It is not a driver for Surface Pro 7+ or other Surface models. Browse the [C/C++/Rust source snapshot](source/) directly, or use the ordered patches to reconstruct complete source trees.

**Status (2026-09-25):** On one Surface Pro 7 running Fedora 42, the maintenance candidate `6.19.8-sfp7cam.maint.fc42.x86_64` booted three times and is now the default. Both cameras captured libcamera frames; Snapshot took front/rear/front photos, recorded decodable video from each camera, and reopened successfully. A timed s2idle suspend/resume was followed by working front and rear captures and photos. The original P1 kernel remains a GRUB fallback. Video quality is still limited by low bitrate, and there is no independently tested second device or complete signed installer. See [candidate device validation](docs/MAINTENANCE-DEVICE-VALIDATION.md) and [historical P1 results](docs/STATUS.md).

This is a **source publication**, not an installable release. The tested Fedora 42 RPMs are unsigned and the runtime helper, service, and modprobe policy are not yet owned by an RPM. Please do not replace your boot kernel solely from these source files. A volunteer preview will follow after packaging and recovery instructions are ready.

## Contents

| Path | Purpose |
| --- | --- |
| [`source/`](source/) | Directly browsable IPU4P kernel C code, libcamera C++ and Snapshot Rust source from pinned runtime versions plus maintenance candidates |
| [`kernel/`](kernel/) | Linux 6.19.8 → IPU4P → Surface Pro 7 → final tested kernel tree; [composition guide](docs/BUILDING.md) |
| [`userspace/libcamera-v0.4-runtime-fixes/`](userspace/libcamera-v0.4-runtime-fixes/) | Ordered libcamera 0.4.0 backports and local fixes, plus Fedora 42 spec overlays |
| [`userspace/snapshot-48-runtime-fixes/`](userspace/snapshot-48-runtime-fixes/) | Snapshot 48.0.1 VP8 bitrate fix and Fedora 42 spec overlay |
| [`runtime/`](runtime/) | Exact P1 one-shot loader, systemd unit, modprobe and WirePlumber settings used on the test device |
| [`docs/FIRMWARE.md`](docs/FIRMWARE.md) | User-side extraction of the required IPU4P firmware from Microsoft's official Surface Pro 7 MSI |

The [tested artifact hashes](docs/STATUS.md) identify local build outputs, but no Microsoft firmware or camera footage is hosted here. The full development log, local machine paths, boot identifiers, and experimental patch reversals are intentionally outside this public repository.

## Source and contribution boundaries

The kernel composition starts from a pinned [linux-surface `v6.19.8`-based commit](docs/BUILDING.md) and includes work by [Ruslan Bay's IPU4P series](https://github.com/linux-surface/kernel/pull/163) and [thisiscamk's Surface Pro 7 port](https://github.com/thisiscamk/sp7-ipu4-camera), followed by the project's tested runtime delta. The final delta is a **flattened build input**, not an attribution-preserving upstream submission. The userspace patch series retains the authors of upstream libcamera backports. See [attribution and licenses](COPYING.md) and [contributing](CONTRIBUTING.md) before submitting any part upstream.

This work used AI coding assistance. Human review, proper authorship, and valid Developer Certificate of Origin sign-off are still required for upstream submissions.

[简体中文说明](README.zh-CN.md)

For development, start with [source maintenance](docs/SOURCE-MAINTENANCE.md) and
[tests](tests/README.md). The original runtime inputs remain available; browsing
snapshots include maintenance candidates; device acceptance is limited to the
exact source and package identities in the linked validation record.

The [maintenance validation record](docs/MAINTENANCE-VALIDATION.md) documents the
refactor, independent review and offline test boundaries; the
[device record](docs/MAINTENANCE-DEVICE-VALIDATION.md) covers the later installation.
