# Copyright, attribution and licenses

This is a **multi-project source collection**, not a new blanket license for all files.

The files under [`source/`](source/) are selected copies of the resulting upstream kernel, libcamera, and Snapshot source. They retain their upstream copyright/SPDX headers and the applicable license texts in each snapshot. Their inclusion does not change the terms described below.

- `kernel/ipu4-next-v6.19/` contains patches from [Ruslan Bay's IPU4P branch](https://github.com/ruslanbay/ipu4-next), preserving their original patch author headers. They apply to the Linux kernel; the resulting kernel uses [Linux's GPL-2.0-only framework and per-file SPDX expressions](https://docs.kernel.org/process/license-rules.html). The IPU4P source repository also has its own LGPL-3.0 license notice; review its original files and notices before republishing a derived kernel tree.
- `kernel/thisiscamk/` is an overlay derived from [thisiscamk's Surface Pro 7 port](https://github.com/thisiscamk/sp7-ipu4-camera) at commit `da7fd41db0e72f87a1a7d192f89aa294f53ab62f`. The overlay and no-binning patch apply to Linux source; original contributor attribution must be retained in any upstream split.
- `kernel/runtime-final-net.patch` is a mechanical difference between pinned Linux source trees. It may contain changes to files with different SPDX expressions. It is a reproducibility input, not a newly authored single patch or a claim that one person wrote all its hunks.
- `userspace/libcamera-v0.4-runtime-fixes/` applies to [libcamera v0.4.0](https://libcamera.org/). Its first six patches are attributed upstream backports. The changed libcamera files retain their [upstream per-file licenses](https://docs.libcamera.org/master/#licensing); libcamera core is generally LGPL-2.1-or-later and IPA files may differ.
- `userspace/snapshot-48-runtime-fixes/` applies to [GNOME Snapshot 48.0.1](https://gitlab.gnome.org/GNOME/snapshot). The changed Aperture source carries GPL-3.0-or-later SPDX notices.
- New project-authored documentation, `scripts/`, host test fixtures and `runtime/` helper/configuration files are offered under the [MIT License](LICENSES/MIT.txt), except portions copied from separately licensed upstream material. Native libcamera tests carry their own LGPL-2.1-or-later/CC0 SPDX notices. This grant does not relicense a kernel, libcamera, Snapshot, firmware, or any third-party patch.

The local userspace patches were prepared with AI coding assistance under the project owner's direction. Their internal development copies used a placeholder `@localhost` identity and some placeholder `Signed-off-by` lines. The public copies omit those invalid sign-offs; no DCO certification is implied. Actual upstream submissions need a human to review the exact source, identify the right author(s), preserve inherited notices, and sign only under the applicable project's rules.

No Microsoft firmware is included. Users must obtain it themselves from the [official MSI](docs/FIRMWARE.md).
