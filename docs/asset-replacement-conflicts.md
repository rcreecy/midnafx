# Replacement conflict qualification

This milestone adds four synthetic tests to the pinned Aurora registry test harness. It exercises the real `register_virtual_replacement`, `find_source_replacement`, unregistration and clearing paths. No game archive, texture pack, image decoder result or GPU device is required. This complements the earlier [sampled file-registration proof](asset-provider-correlation.md).

Workers remain paused during these cases, so registration selection is intentionally separate from payload availability. A selected ID with an empty handle does not prove a replacement was bound. Live conflicting pack draws, async publication races and total observer overhead remain separate gates.

## Rules under test

- Higher numeric priority wins within the same source key, even if a lower-priority entry was registered later.
- Equal priorities choose the most recent registration. Removing it exposes the earlier entry.
- Exact source keys take precedence over wildcard source keys before comparing priorities. Removing the exact registration exposes the wildcard.
- Removing all entries leaves no selected registration. Re-registering produces a fresh ID. Stale registration tokens do not remove new entries, including after clearing the registry.

These rules describe the pinned implementation. They are not inferred from filenames or screenshots. The existing file-backed live proof remains narrow to the two pumpkin replacements and does not qualify all provider types or priority combinations.

## Validation

The Windows `RelWithDebInfo` native streaming executable passes **10/10** tests in 0.074 seconds, including all four added cases and the six existing streaming/DDS tests. Ten targeted repeats pass **40/40** new-case executions. Zero-duration entries reflect timer resolution, not a performance measurement. Full stdout and XML remain under `build/asset-conflict-*`.

The [metadata summary](evidence/asset-conflicts-2026-10-20/summary.json) records the tested executable, patch and log hashes, test names and results. The test patch reverses and reapplies cleanly while preserving the existing experimental host edits. The existing Python asset suite passes **14/14**. No native macOS conflict test execution or live conflicting-pack rendering is claimed.

## Reproduction

Apply the test-only patch with `python tools/asset-draw-host-patch.py --selection-tests --apply`. The helper checks Aurora revision `7f2801cd0133c9333eadb4e2e6b24100c328d328`, preserves unrelated edits, and supports reversal with `--selection-tests --reverse`. This patch changes only the existing upstream test source. The experimental draw host patch is not required by these registry cases.

Build Aurora's `texture_replacement_streaming_tests` target in a configured native host build. In the existing Visual Studio multi-project tree, invoke the generated `build/runtime-host/extern/aurora/tests/texture_replacement_streaming_tests.vcxproj` directly with MSBuild and configuration `RelWithDebInfo`, platform `x64`; the top-level generated solution does not expose this excluded test target by its short name. Run its executable with `--gtest_filter=*MidnaFX*` for the four new cases, or without a filter for the complete streaming suite.

Ordinary package CI checks forward applicability of the test patch on the clean pinned Aurora checkout. That check does not compile or execute the native host registry tests. Production mod code, settings and release artifacts do not change.
