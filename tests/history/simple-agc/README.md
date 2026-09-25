# Historical AGC negative control

This frozen extractor/harness checks the original runtime 0017 versus 0018
change, including defensive NaN/negative-MSV guards that normal histograms cannot
produce. It intentionally uses handwritten types and is **not** the forward
refactoring test. The candidate source hash is fixed; use the native tests for
modified classes. Extraction is local, with no import of a sibling test's runner.

```sh
python3 scripts/source.py prepare --component libcamera --profile fedora42-runtime --output ../agc-0017
python3 scripts/source.py prepare --component libcamera --profile fedora42-runtime --output ../agc-0018
git -C ../agc-0017 apply --reverse --index "$PWD/userspace/libcamera-v0.4-runtime-fixes/patches/0018-ipa-simple-converge-startup-exposure-by-measured-deficit.patch"
python3 tests/history/simple-agc/run.py --baseline ../agc-0017 --source ../agc-0018 --output ../agc-history-result
```

Expect 77 checks, including 60 steady-state comparisons. The baseline dark update
requests 758 exposure lines from 632; 0018 requests 1264. Existing output is
rejected. The report binds source, headers, extractor, compiler and sanitizer
settings. Source 0017 in this example is a deliberately modified disposable tree;
its original prepare identity no longer describes it after the reverse patch.
