# Maintenance candidate: single-device validation (2026-09-25)

The maintenance candidate is installed as the default on one Surface Pro 7
(IPU4P `8086:8a19`, Fedora 42 x86_64). Three candidate boots succeeded: two
selected with a one-time GRUB entry, then one from the persistent default.
The original P1 kernel and its conditional loader remain installed as GRUB
fallback entries. This is a device result for this exact build, not a claim
about other Surface Pro 7 machines or a downloadable release.

| Installed component | Identity |
| --- | --- |
| Kernel source tree | `64eed5318e94ccd883b10456e0606d1a9ce47451` |
| Kernel config SHA-256 | `2001c9ef43d83f1ba54c02574c5a6257a71affe227fd869b9cb2f66b3c209491` |
| Kernel release | `6.19.8-sfp7cam.maint.fc42.x86_64` |
| Kernel RPM SHA-256 | `bc0eafb834976a4d2bde534735a7d033659a55d932c55a798df5413d390c449f` |
| Fedora libcamera source tree | `c8e79cb2d3c268d72f2c6e9ea5d574bac720cf5b` |
| libcamera / IPA RPM version | `0.4.0-4.sfp7.12.fc42.x86_64` |
| libcamera / IPA RPM SHA-256 | `6c78dc0e2dec33dc4bab7cb5e89b4a1fb4b217864a4fa654c495a02616e2a10c` / `5bee179bcc893bc4b08b80941f37347145a5e881ad1c21860133d8d8fc5bd02e` |
| Snapshot | Existing `48.0.1-1.sfp7.1.fc42.x86_64`; the maintenance source changes only a comment |
| Candidate loader SHA-256 | `7a2c5bb9847e3a2ed6c1f10e3e249a99d42540d8dd63537226eed5711ebed83d` |

The complete source tree was checked by Git tree identity on the target's
case-sensitive filesystem. This Fedora 42 build used the original P1 kernel
configuration after `olddefconfig`; its config hash equals the P1 build's.
The earlier isolated Docker build used a different toolchain-detection config
(`50a5e267…`), so its binary hashes are **not** attributed to the RPM above.
The target produced `bzImage` and all 4,971 configured modules. The RPM payload
contained exactly 4,971 modules, all with the candidate `vermagic`, a build-time
kernel-key signer and SHA-512 module signatures. `modules_check`, RPM digest
verification, side-by-side install dry run and installed-file verification
excluding generated `%ghost` files passed. The candidate loader pins the seven
signed camera module digests from that installed RPM; it retains the one-attempt
load rule and exact kernel-release condition.

| Device check | Observed result |
| --- | --- |
| Boot and load | All three boots reached the candidate release; the conditional loader exited successfully, `/dev/media0`, `/dev/ipu-psys0` and 55 video nodes appeared |
| libcamera | First boot: 120 consecutive front frames and 120 rear frames, about 29 and 15 fps; six alternating reopen runs captured 30 frames each |
| Snapshot photos | One front → rear → front session saved three distinct 1920×1080 JPEGs; the same sequence passed after suspend/resume |
| Snapshot reopens | Three independent app starts found both cameras and usable controls |
| Snapshot videos | Front and rear each recorded about 18 seconds of 1920×1080 VP8/Vorbis WebM; both files decoded completely |
| s2idle | One RTC-timed 30-second suspend resumed in the same boot; each camera then yielded 60 frames and Snapshot photos passed |
| Persistent default | A third reboot without a one-time override loaded the candidate automatically; each camera yielded another 30 frames |
| Post-test state | Kernel taint stayed at baseline 4; four IPU bus devices returned to `auto` / `suspended`; installed kernel, libcamera and Snapshot non-ghost RPM files verified |

The video bitrates remained low: roughly 346 kbit/s front and 342 kbit/s rear.
These tests prove recording and complete decode, not subjective image quality.
The previously observed SoundWire audio UBSAN and unrelated OV7251 I²C probe
error also appeared on this boot; no new camera-related kernel failure was
observed. Secure Boot was disabled. There is no Live USB or second SP7 test.
Focus speed, startup brightness and image quality were not remeasured by this
maintenance regression run.

The old P1 GRUB entry remains available. Its saved-entry identifier is
`e75b9ff03b044ab988bf198b8c075585-6.19.8-sfp7cam.p1.fc42.x86_64`;
`grub2-set-default` can restore it from a working Linux boot. The P1 libcamera
RPMs are retained separately for a full userspace rollback. The candidate
kernel RPM, userspace RPMs and loader are local validation artifacts, not
published or signed distribution packages. Packaging the loader and desktop
settings, documenting offline recovery, distribution signing and independent
hardware confirmation remain release work.

The internal metadata archive contains logs, package identities, outcomes and
photo/video paths and hashes, but no RAW frames, photos or videos. Its SHA-256
is `30381f8420e8e33216fa9716de0d81212581fada9d35afe0ad53585df9e10626`.
The [offline refactor checks](MAINTENANCE-VALIDATION.md) and [historical P1
results](STATUS.md) are separate records.
