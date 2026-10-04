# v1 readiness gate audit

This is the historical **published v1.0.0 engineering audit**. The subsequent
development pass adds automatic structural qualification and product profiles.
Its stricter product acceptance status is **not complete**; see
[the product-quality review](product-quality-review.md) and
[automatic smoothing](automatic-smoothing.md). The exact-allowlist release
boundary and completed gates below describe the released package, not the new
development classifier or a completed product-quality validation pass.

This audit separates completed engineering gates from optional coverage expansion. It does not
convert a build, deterministic test, or source review into runtime evidence. The runtime results
below use the source-matched Windows host unless stated otherwise.

## Milestone disposition

- **M1-M4.2 grading, presets, diagnostics, and detail:** implementation, package checks, shader
  compilation, and Windows visual checks are complete. Broader scene sampling is release
  qualification rather than an unfinished architecture gate.
- **M5 semantic Twilight profile:** normal and active-Twilight state selection, transition, and
  render application passed live Windows testing. Twilight-spot state 2 is covered
  deterministically and intentionally keeps the general look. A live state-2 capture remains
  useful coverage, but the feature is opt-in and this does not block v1.
- **M6 native environment control:** closed as a negative architecture gate. The pinned SDK has no
  mod-scoped ownership and restoration contract for the game's continuously interpolated fog,
  bloom, lighting, overlays, or particles. MidnaFX must not add direct writes for v1. M10's
  separate depth-aware fullscreen foundation is the supported route for future atmosphere work.
- **M7 topology and smoothing foundation:** Gate 2 passed. Exact allowlisted rigid and skinned
  models passed topology, conflict, mutation, skinning, restoration, and load-time cost checks.
- **M8 geometry coverage:** complete for the v1 experimental scope. Two naturally instantiated
  curved static actors, one controlled static actor, and Link's skinned body are fingerprinted and
  fail closed outside their exact validated representations. Static and skinned smoothing remain
  default off. Another static topology class and another character are coverage expansion, not a
  prerequisite for shipping the existing allowlist.
- **M9 camera:** complete for the opt-in v1 scope. Native first person and real Fyrus combat
  proved fail-closed special-mode behavior and clean exploration recovery. Wider visual tuning
  remains post-v1 coverage.
- **M10 depth foundation:** Windows D3D11 depth availability and visual reconstruction passed,
  including HUD exclusion and live resize. No native environment state is changed.
- **M11 depth of field:** complete for the default-off experimental v1 scope. The quality pass
  added adjustable radius, temporal autofocus, and silhouette-safe near compositing; Windows
  runtime execution, movement, resize, HUD exclusion, and clean shutdown pass. A fresh matched
  image of the corrected silhouette and live Metal output remain explicit release-note limits.

## Geometry release boundary

The v1 geometry contract is deliberately narrow:

1. Match archive, resource name, source or rebuilt byte fingerprint, topology hash, vertex counts,
   envelope state, normal format, stride, and fraction before writing.
2. Reject malformed topology, unsupported layouts, invalid indices, non-finite normals,
   ambiguous faces, and unresolved normal-index conflicts.
3. Process once per resource lifecycle. Restore original normals on configuration disable,
   archive deletion, mod reload, and graceful shutdown.
4. Keep rigid and skinned controls independent and default off.
5. Do not infer safety for any model outside the exact allowlist.

Existing live evidence covers shared instances, concurrent resources, archive unload/reload,
in-process disable, mod reload, graceful shutdown, GPU skinning, two native lighting layers, and
bounded load-time memory. Expanding the allowlist requires the same fingerprint, close visual,
lifecycle, and performance proof; it is not a generic switch.

## Ship status

The final combined-feature regression, package verification, and release
correctness review are complete. Intel Mac packages are build- and
contract-tested, but live Metal execution and GPU performance remain
unvalidated hardware risks and are explicit in the v1 release notes.

## Final combined pass (2026-10-03)

The source-matched Windows D3D12 host ran grading, detail, exploration camera,
6-pixel depth of field, and allowlisted static smoothing together at 1216x896.
Grading submitted and encoded 256/256 draws with 11.80/26.10 microsecond
median/p95 callback time. Pumpkin and beehive smoothing completed in 517 and
120 microseconds with zero index conflicts. Camera modifiers reached their
configured FOV and `-6.00`-degree latitude offset; depth-of-field compute and
composite executed. Graceful shutdown restored both normal arrays and unloaded
all mods. No MidnaFX, WebGPU, validation, fatal, or non-finite error appeared.

Windows packaging, Dawn shader validation, and all 13 tests pass. The release
correctness review is recorded in `docs/notes/v1-release-correctness-review.md`.
The Windows v1 engineering gates are closed. Live Intel Mac/Metal behavior and
the missing fresh screenshot of the final depth-of-field edge correction remain
disclosed release risks, not claims of validation.

Native M6 environment overrides, global geometry smoothing, additional character allowlists,
subdivision, mesh replacement, and material replacement are post-v1 work. They are not hidden
requirements for the implemented default-off features.
