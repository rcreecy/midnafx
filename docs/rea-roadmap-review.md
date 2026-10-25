# REA opportunities for the MidnaFX remaster program

Review date: 2026-10-24; implementation checkpoint: 2026-10-25. The execution
section distinguishes completed qualification and the first material profile
from the remaining proposed work.

## Recommendation

Use REA to answer questions about the actual packaged module, native host and
live game state. Continue using ordinary source tools for architecture and edits:
MidnaFX, Dusklight and Aurora source are already available. Reverse engineering
their complete executables would usually be slower than reading that source.

The largest implementation opportunity is a short material-state investigation
followed immediately by useful, reversible material profiles. The largest quality
opportunity is verifying that the binary and runtime being tested actually match
the intended source, SDK and ownership assumptions. REA complements the existing
asset parsers, lifecycle hooks, pixel tests and timing collector.

Ranked by evidence that REA uniquely adds, the order is: actual release-artifact
audits; unresolved live material/ownership probes; selected lifecycle/allocator
probes. That differs from product implementation priority: R2 material profiles
remain the next visible feature. Use the release audit as a small reusable gate,
not a reason to defer feature development.

## What is available on this host

- The live Codex MCP connection responds as REA 6.7.0, advertises 207 tools and
  reports aligned server/catalog identity.
- Ghidra 12.1.2 is available at `C:\research\ghidra_12.1.2_PUBLIC`, using the
  installed Microsoft JDK 21.0.11. Its headless launcher reaches the usage entry
  point. It also opens and analyzes an owned Windows x86-64 PE fixture: assembly,
  pseudocode and references for its known arithmetic function are retained in the
  [qualification evidence](evidence/rea-qualification-2026-10-24/README.md).
- Frida device discovery succeeds and reports the local system. A bounded spawn
  observation on the owned fixture reports 78 calls and zero arithmetic mismatches.
  Subsequent checks find no open session for its exact ID and no remaining PID.
  The exported observation itself says `running`; cleanup is established by those
  separate follow-up observations, not by that field. Controlled attachment to
  the isolated game host also observes 114 warm/restore command pairs after mod
  reload, with separate session-retirement and loaded-module identity receipts.
  This does not qualify arbitrary-process attachment or measure GPU completion.
- Ghidra-backed function analysis, call tracing, p-code value tracing, instruction
  inspection, recovered type inspection and function comparisons are advertised.
  They require opening a supported target; advertisement alone does not establish
  successful analysis of a particular MidnaFX artifact.
- Evidence bundles, explicit unknowns and artifact comparison are available.
  Windows Ghidra analysis and local Frida discovery do not establish Mac runtime
  support. Hopper is unavailable on Windows; IDA and Kuna are not configured.
  Opening the actual CI Mac module on 2026-10-25 is rejected: the available Windows
  Ghidra provider accepts native PE and selected raw 6502 targets, not Mach-O.
  Consequently the advertised `inspect_macho` workflow is unavailable through
  current host providers. The [CI artifact receipt](evidence/rea-artifacts-2026-10-24/ci-682dacd/README.md)
  retains that result and the successful Windows/Mac build and package checks.
- REA's pwntools layout/core inspection and persistent GDB/Rizin workflows have
  Linux/provider requirements. They are not proposed as current Windows tools.
- `inspect_native_load_image` supports DOS MZ, COM and explicitly selected raw
  images. It is not a Windows PE import/dependency auditor. Keep the existing
  package validator for archive/PE architecture checks and Windows `dumpbin`
  for PE imports/exports; Ghidra's observed PE analysis is a separate capability.

The owned fixture binary was opened and its process was instrumented. No game
process was instrumented for this qualification. The retained bundle records
provider-specific limitations; the fixture result is not a general product gate.

## Ranked product work

| Priority | Roadmap | Proposed result | Where REA adds value | Relative effort |
|---|---|---|---|---|
| 1 | R2 | A verified material-control contract, then useful profiles for three qualified material identities/classes | Inspect live TEV/material/light state with short Frida probes; corroborate uncertain compiled call/value paths with Ghidra | Medium; highest direct feature value |
| 2 | R0/R1 | Explain remaining transition failures and material timing tails; strengthen ownership/restoration proof | Correlate bounded lifecycle events, native state and selected allocator paths without repeatedly rebuilding the host | Small to medium per question |
| 3 | R5 and macOS support | Audit the actual release packages, native imports and loaded host identity | Archive inventories and focused function comparisons complement existing Windows PE import/export inspection and catch stale or incompatible artifacts | Small to medium; high release-quality value |
| 4 | R3 | Expand geometry coverage without shared-buffer or skinning regressions | Verify the live normal/vertex buffers and matrix relationships consumed by qualified draws | Medium; content qualification still dominates |
| 5 | R4 | Make water, depth, AA and material exclusions more coherent | Observe CPU-side input selection and pass ordering at uncertain native boundaries | Medium; pixel and motion testing remain decisive |

Effort ratings are planning judgments, not measured speedups. REA has not yet
demonstrated an implementation-time reduction on this project.

### 1. Rendering contract and visible material profiles

Start with the existing original pumpkin and pot identities plus one explicitly
qualified foliage material. Use source to locate the owning functions and
symbols. Use `analyze_function`, `trace_call_path`, `trace_native_values` and
`inspect_native_instruction` only where shipped behavior remains uncertain.
Recovered Ghidra types are analysis observations, not automatically authoritative
C++ layouts; corroborate them against matching source/debug metadata and bytes.

Use a bounded Frida probe to capture material identity, TEV stage definitions,
texture selection, relevant light inputs and callback ordering. This can avoid
rebuilding the native host for each exploratory field or counter. Distinguish
parameters baked into generated pipelines from values that can change per draw.
Record which normals, UVs, tangents, light/view inputs and shader ownership are
actually present. Unknown inputs remain unknown.

Implement profiles through the narrowest supported interface after that audit.
Begin with supported authored color/response changes and foliage protection;
normal maps, specular lighting and PBR require their missing inputs and a complete
rendering path. The existing pumpkin's known SCALE_4 restore is not a generic
snapshot/restore contract for arbitrary materials.

Acceptance: useful visible changes on three qualified identities/classes; original
and replacement textures work; alpha, unrelated materials and HUD remain correct;
disable, archive destruction, return and mod reload restore the owned state;
unsupported models bypass the feature. Retain matched images, motion checks and
native timing controls. Stop exploratory probing once the contract is sufficient
to implement this slice.

### 2. Focused lifecycle and performance diagnosis

The existing archive evidence is now reconciled in the
[lifecycle ledger](evidence/material-lifecycle-2026-10-22/README.md).
The retained Ordon–Fishing Pond–Ordon trial clears the binding at 17,982 balanced
applies/restores, binds the returning model, and reaches shutdown with 515,310
balanced applies/restores, zero rejected stages and `all mods unloaded` logged.
Its process exit code remains unknown. Reuse this route-specific evidence
instead of commissioning another identical experiment solely to fill a stale
roadmap entry.

For the unresolved Hyrule Field black transition, record the native scene/room/
player creation phases, overlap state, both fade mechanisms and archive lifetime
around a matched control. A trace can distinguish a progression/spawn fixture
problem from a restoration failure. Do not infer the cause from the final buffered
log or from one fade field. The independently reproduced full-queue decompressor
repair does not establish the cause of this transition or the historical stall.

For material timing tails, collect targeted event/call observations during settled
draws, active sampling and replacement reload. Instrument selected JKR/allocator
boundaries only with explicit coverage and bounded output. Hooks on one allocator
are not complete process-wide allocation accounting. Frida itself changes execution
and adds overhead: use it to find causes, then measure the resulting implementation
with the native collector and matched controls. GPU timestamps or an appropriate
native GPU profiler are still required for GPU cost.

Acceptance: a specific fault mechanism or a clearly bounded unresolved result,
an implementation/test at the responsible layer when justified, and valid controls
covering the missing active/reload phases. Keep failed trials.

### 3. Release and cross-platform artifact audit

Use the existing ZIP/package validator for `.dusk` structure and manifest checks.
REA's archive inventory can add content-addressed package comparisons. Inspect the
extracted Windows `mod.dll` and Intel Mac `mod.so`, with their matching native host
and symbols where supported. Check architecture, required imports, exports and
the compiled paths affected by a change. `compare_functions` compares observed
analysis facets; it does not prove behavioral equivalence.

For Windows PE files, the existing validator checks PE signature and AMD64
architecture; use `dumpbin /imports` and `/exports` for import/export inventories.
Do not substitute `inspect_native_load_image`: that operation is limited to
DOS MZ, COM and raw-image loading, despite the shared historical MZ signature.

Where a runtime probe is warranted, verify the actually loaded host/module identities.
The prior renamed-`dusklight.exe` experiment loaded a second host image and
invalidated performance measurements; keep the repaired launcher's mandatory
identity checks. REA should extend those checks, not replace them.

Static Mach-O inspection and `trace_dylib_resolution` can improve Mac packaging
confidence from Windows where the tool supports the selected artifact. Required
system libraries outside the supplied root remain undetermined. Static inspection
cannot establish successful loading, gameplay or Metal rendering on macOS.
Keep Mac support best effort, continue package/portable checks, and do not introduce
a remote hosted Metal gate.

### 4. Geometry and character consistency

Use temporary observation probes to verify which rebuilt/source normal buffers,
matrix groups and shared model instances feed the actual draw, including animation
and scene changes. Use the existing BMD/topology parsers for asset structure;
Ghidra's recovered types and native bytes can corroborate an uncertain loader
boundary. REA is not a replacement BMD/DDS authoring or model-quality tool.

Turn useful observations into repeatable native qualification tests. Extend one
asset class at a time, retaining seam, silhouette, skinning, authored-normal and
restoration checks. New meshes, animations and artistic review remain content work.

### 5. Environment integration

Trace uncertain CPU-side depth source, camera/light selection, water classification
and capture/replay ordering. This can locate incorrect exclusions or stale inputs
before another broad shader adjustment. GPU shader compilation, image comparisons,
water-edge tests and temporal/shimmering checks still establish visual behavior.
REA cannot infer a studio remaster's rendering techniques or author a complete
lighting/reflection system from an albedo pack.

## Execution order

1. Owned-fixture qualification is retained: known native arithmetic is analyzed,
   evidence is exported, a bounded Frida observation matches 78 calls, and follow-up
   checks confirm session retirement and process disappearance. This does not
   qualify arbitrary attachment or whole-host decompilation. Preserve the distinction
   between the exported `running` state and separately observed cleanup.
2. A [Windows artifact/import baseline](evidence/rea-artifacts-2026-10-24/README.md)
   now checks the final packaged module and selected host. The reconciled earlier
   archive round trip is retained, and the first warm material profile is
   implemented. Its [ledger](evidence/material-profile-2026-10-24/README.md)
   separates candidate pixel comparisons, final native reload and the subsequent
   archive round trip. A bounded post-reload Frida observation confirms 114
   warm/restore command pairs; loaded module and host hashes match the baseline.
3. Exercise lifecycle and active/reload controls for that profile, investigate any
   repeatable tail regression, then extend to the next qualified material classes.
4. Audit the resulting Windows/Mac release artifacts and broaden geometry/environment
   coverage using the same evidence-to-feature loop.

For each investigation, retain artifact SHA-256, source/SDK revision, relevant
function/address or resource identity, REA Evidence IDs, observed/inferred/unknown
status and the test that can resolve the remaining question. Export reusable
evidence/snapshots before closing a native session. Reuse matching analysis instead
of repeating broad scans. Keep the ledger small; a full reconstruction-obligation
framework is unnecessary for ordinary source development.

## Source anchors

- [Remaster program](remaster-program.md): R0–R5, visual target and platform policy.
- [Material control](material-control.md): narrow current write/restore contract.
- [Frame timing](frame-timing.md): measured tails and missing allocation/GPU evidence.
- [Geometry investigation](geometry-investigation.md): authored normals and live skinning.
- [Water investigation](water-investigation.md): exact classification and missing scene coverage.
- [Host identity failure](asset-host-name.md): why actual loaded binaries matter.
- [Package validator](../tests/package_contract.py): existing archive and architecture checks.
