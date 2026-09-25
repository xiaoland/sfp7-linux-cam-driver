# BLC/LUT source comparison

```sh
python3 tests/blc-lut/run.py --baseline /path/to/baseline --candidate /path/to/candidate --output /new/result
```

Compiles the complete BLC/LUT class declarations and implementations, plus the real
IPA/statistics data structures, after removing only include/pragma directives.
Logger, framework/registration and platform boundaries are supplied by fixtures.
Checks startup, valid/empty statistics, exposure/gain changes, only-decreasing
estimation, reconfiguration and LUT output under ASan/UBSan; compare observable
outputs across both trees. No class layout is hand-copied into a fixture.
This does not replace complete libcamera compilation or image/IPA-thread tests.
