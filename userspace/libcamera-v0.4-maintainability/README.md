# Simple IPA maintenance layers

The original eighteen runtime patches remain unchanged. `tests.series` adds native
Linux tests using the actual AF/AGC classes and dependency graph; `series` contains
production refactors applied after those tests. Editing the generated browsing
snapshot is not the source of truth.

The source input profiles make the comparison explicit:

| Profile | Ordered layers |
| --- | --- |
| `runtime-browse` | upstream archive → runtime → native tests → maintenance |
| `fedora42-runtime` | upstream archive → Fedora compiler fixes → runtime |
| `fedora42-baseline` | upstream archive → Fedora compiler fixes → runtime → native tests |
| `fedora42` | upstream archive → Fedora compiler fixes → runtime → native tests → maintenance |

See [tests](../../tests/README.md) for complete builds and per-callback comparisons.
Hardware acceptance of the original runtime does not transfer to these changes.
The historical Fedora RPM spec overlays still describe the original runtime set.
A candidate package needs its own source identity and validation before deployment.
