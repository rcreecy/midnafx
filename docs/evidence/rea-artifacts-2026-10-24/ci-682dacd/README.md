# CI artifacts for the warm material profile

On 2026-10-25, [run 40205847046](https://github.com/rcreecy/midnafx/actions/runs/40205847046)
passes at code commit `682dacde18b1066539c4920e9f842f9773fd42ac`:
Windows builds/packages and passes 21 tests, including AA pixels; Intel macOS
builds/packages and passes 20 tests. The Mac job excludes native AA/Metal
execution under the working-host policy. Release publishing is skipped.

The downloaded packages both pass `tests/package_contract.py ... 1.2.0` on this
Windows host. `summary.json` records the exact package and extracted module
hashes. These CI artifacts differ from the locally executed final package in the
parent baseline; their provenance and validation must remain separate. No game
binary or third-party asset payload is retained here.

The CI Windows module imports the same 80 named host symbols. All its named
imports are present in the local baseline's per-DLL import sets; the only removed
name is CRT `_fdclass`. Hints and addresses differ and are excluded from that
comparison. This extends the existing static name-presence check, not ABI,
loader-search or gameplay qualification of the CI binary.

REA rejects opening the Mac Mach-O module through Windows Ghidra: its Windows
provider currently accepts native PE and explicitly selected raw 6502 targets.
Hopper is unavailable on Windows, and IDA/Kuna are not configured. The unchanged
`rea-macos-open.json` retains the response. No native session or analysis snapshot
is created. Therefore `inspect_macho` cannot be used on this host through the
currently available providers. The successful Mac package/build tests remain
valid; Mac loading, gameplay and Metal rendering remain unverified and best effort.
