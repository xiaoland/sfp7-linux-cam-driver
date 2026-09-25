# Tests and what they prove

- `python3 -m unittest discover -s tests/source -v`: real tiny Git/archive fixtures
  test replay, file coverage, type/mode drift, failure isolation and snapshot ownership.
- [BLC/LUT comparison](blc-lut/README.md): complete class sources, real data structures and framework substitutes.
- [Loader contracts](runtime/README.md): fake filesystem and command boundaries in an isolated Linux container.
- [Driver models](driver/README.md): selected actual functions with controlled
  dependencies; no real IRQ/FW/PM behavior.
- [Historical AGC](history/simple-agc/README.md): original 0017/0018 regression,
  including artificial invalid-MSV inputs; frozen source identity.
- Native Simple IPA tests: compile real AF/AGC translation units and headers,
  generated IPA interfaces, controls, frame queues and libcamera libraries.

## Native equivalence test (Linux)

```sh
python3 scripts/source.py prepare --component libcamera --profile fedora42-baseline --output ../baseline
python3 scripts/source.py prepare --component libcamera --profile fedora42 --output ../candidate
python3 scripts/test-simple-ipa.py --baseline ../baseline --candidate ../candidate --output ../comparison
```

Use a fresh output directory and clean, committed source trees. The runner requires
identical test sources, builds both complete trees with ASan/UBSan, runs the two
Meson tests and compares every emitted lens/exposure/gain request. It reports the
first differing row and records source trees, test hashes and commands. Both trees
include the native-test overlay; only the candidate includes production maintenance
patches. `fedora42-runtime` preserves the pre-refactor code without native tests.

A Fedora 42 build environment needs gcc-c++, git, meson, ninja-build,
python3-jinja2, python3-ply, python3-pyyaml, libyaml-devel, openssl, openssl-devel,
systemd-devel, libevent-devel, libdrm-devel, libjpeg-turbo-devel, libyuv-devel,
libatomic, libasan, libubsan, pkgconf-pkg-config and diffutils. Tests also cause the
upstream build to enable virtual/vimc components. Downloads of fallback subprojects
are disabled; install missing dependencies explicitly. The original package recipes
are historical; these commands do not install anything on the camera device.

AF covers missing/invalid controls, startup/settle sampling, flat/zero/endpoint/peak
scans, mid-scan reconfiguration, frame-number gaps, cooldown and sustained loss,
and recovery windows. AGC covers empty histograms, 95/96/97 valid callbacks,
measured feedback delays, threshold exit, actuator limits and minimum steps.
The short start/stop wrapper is reviewed separately: it does not reconfigure the
algorithms. Tests do not load that wrapper or model lens mechanics, control-delay
queues, image quality, IPA threading or hardware. NaN/negative MSV defense belongs
to the historical white-box test; normal histogram inputs cannot reach it.

`tests/Containerfile` pins the Fedora base image used for this work. Build it with
`docker build -f tests/Containerfile -t sfp7-camera-tests .` and copy prepared trees
into a container, or mount them when Docker runs on the same host. No camera
devices are needed. Files copied from another host may retain that host's numeric
owner. Assign the disposable copies to the container build user before testing;
the runner isolates Git configuration and does not bypass Git's ownership checks.
Package repository updates can still change the toolchain;
retain the comparison logs and exact source identities when reporting results.
