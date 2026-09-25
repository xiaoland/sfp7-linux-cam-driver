# Loader contract tests

Run `python3 -m unittest discover -s tests/runtime -v` as root **inside the isolated
Linux build container**. Other environments explicitly skip these cases.
The test copies the loader into a temporary directory, maps its filesystem roots
there and replaces uname/modinfo/sha256sum/modprobe/journalctl/sleep with command
fixtures. It never loads a module or writes real sysfs, firmware or `/run` state.

Expected module paths/digests/order are frozen from the original P1 release in
`p1-identities.json`. They are not generated from the candidate. Cases exercise
identity rejection, successful ordering, MMU pin placement, failure cutoff and
one attempt per boot. Digest computation itself is mocked; source identity and
real module signatures remain separate from this behavioral model.
