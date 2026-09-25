# Driver lifecycle models

```sh
python3 scripts/test-driver-lifecycle source/kernel
python3 scripts/test-driver-queue source/kernel
```

These runners compile the selected **actual source functions** with handwritten
kernel/framework/FW stubs. The lifecycle runner lists its extracted functions in
`SUITES`; the queue runner extracts `__buf_queue`. Both reject missing definitions.
When a production helper is added, include it in that list or supply an explicitly
documented boundary stub. A helper must not silently disappear from the test.

The tests cover OPEN/START/STOP/CLOSE failures, stream-start rollback, verification
handoff and incoming/active ownership. The P1 stream configuration's four
`*_discarded` fields are represented and asserted at OPEN; an older internal
harness omitted them and did not compile this source. Input/output operation
sequences are checked, not just compilation. The fixture still contains only the
ABI subset used by these functions; this is not a production struct definition.

No real firmware, IRQ, kernel locks, PM or sensor runs here. The runners use
warnings-as-errors without sanitizer flags. Full kernel compilation and affected
hardware paths remain separate requirements. The original model inputs were
preserved from the internal development repository at `fa91d87`; no target logs
or private paths are needed to run them here.
