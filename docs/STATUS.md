# Validation status and artifact identities

Tested on **one** Surface Pro 7 (IPU4P `8086:8a19`) running Fedora 42 x86_64. The P1 kernel `6.19.8-sfp7cam.p1.fc42.x86_64` was installed as a separate package and booted twice. On the first boot, front and rear sensors each yielded three distinct RAW frames; libcamera delivered 120 frames from each camera; Snapshot captured three distinct 1920×1080 photos in front/rear/front order and about 17 seconds of decodable 1920×1080 VP8 video from each camera. On the second boot, the one-shot service loaded the modules without a manual command, rear libcamera captured 300 frames, and Snapshot reopened three times with both cameras available.

The P1 videos were decodable but had low total bitrates (about 332 and 321 kbit/s); their subjective quality still needs review. The P1 kernel has **not** repeated a suspend/resume test. A predecessor kernel on the same device did pass one s2idle/resume and front/rear/front photos afterward; that result does not transfer to P1. No second Surface Pro 7 has been tested.

These exact local build artifacts were used or built during the single-device work. They are **not hosted as a release** here and the RPMs have no distribution signatures:

| Artifact | SHA-256 |
| --- | --- |
| `kernel-6.19.8_sfp7cam.p1.fc42.x86_64-3.x86_64.rpm` | `8d07738036eda29bbd527d1bddfb861f47d90527c9f0c87cac5f9a36d0698012` |
| `kernel-6.19.8_sfp7cam.p1.fc42.x86_64-3.src.rpm` | `6adddbe47d8a0c7142a48787522c3f9c11676e4be6e1b7be8cd8f36f0f6550ea` |
| `libcamera-0.4.0-4.sfp7.11.fc42.x86_64.rpm` | `3e1191292d9a659f9be40c248b89bc8e2d9ec7cdda1712885432c338dbf3018b` |
| `libcamera-ipa-0.4.0-4.sfp7.11.fc42.x86_64.rpm` | `9c3d423581fda52734624f862cd8b0a1fc5373337e60bc81099d90ea820632da` |
| `libcamera-tools-0.4.0-4.sfp7.11.fc42.x86_64.rpm` | `57a675c5aaaf65479e3476be2d9e8e1577ab3c543ed06c6c1d66e7514e581d10` |
| `libcamera-0.4.0-4.sfp7.11.fc42.src.rpm` | `e342340daf265696bf9acfaadf87680d4d6eb5ba59ef0bd6fa7b055433047489` |
| `snapshot-48.0.1-1.sfp7.1.fc42.x86_64.rpm` | `e41544c415a8afe63923023b4169d6365779d7858d153903d42285af47d0a9cb` |
| `snapshot-48.0.1-1.sfp7.1.fc42.src.rpm` | `25092453a1f3d12ab71c7504a4b95ad120bfe43437ed1f474344d31596240f73` |

The remaining volunteer-preview tasks are: package the one-shot loader, service, modprobe and WirePlumber settings; document offline recovery and rollback; decide signing for Secure Boot and RPM distribution; retest P1 suspend/resume and video quality; then seek independent SP7 hardware validation. The existing development machine had GRUB fallback entries but no Live USB. Surface Pro 7+ is outside the test matrix.
