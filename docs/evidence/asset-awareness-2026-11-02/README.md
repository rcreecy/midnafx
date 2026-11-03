# Asset-awareness coverage receipt — 2026-11-02

The final offline scan covers the supplied extracted game's entire `files` tree,
including resource archives, nested containers, UI, animation, audio, movies and
other payloads. It does not inventory runtime-generated memory or the `sys`
executable area. This is a bounded best-effort asset-awareness closure, not proof
of full live material or actor control.

## Result

- **3,656 input files**, with every stored-byte SHA-256 rechecked after export.
- **2,095 archives**, including **252 nested archives**; maximum observed depth 2.
- **25,600 resource records**, with unique scoped IDs and zero scan failures.
- **3,626 model resources** parsed: 3,602 BMD3, 23 BDL4 and one BMD2.
- **13,375 materials** and **19,493 texture-table/standalone texture entries**.
- **18,488 source keys**, representing **5,453 distinct keys**. Shared entries
  retain separate material/model relationships.
- **4,251 texture entries** have exact-key candidates in the local extracted
  Henriko TP4K 4.0d filename inventory. This is not selected-provider evidence.
- **1,005 external texture sources across 178 resources remain unresolved**.
  The catalog preserves their table indices and names instead of inventing hashes.
- **60 standalone palette warnings** retain the native zero TLUT hash after a
  bounded palette check. There are no corresponding model-table fallback cases.

Inventory completeness is true. Supported metadata completeness is false because
external texture bindings remain unavailable. Another 20,018 resources have byte
identity only; their semantic relationships are not parsed. The `.arc` member
`Object/HomeBtn.arc/dat/speakerse.arc` is not RARC and remains identity-only.
All 252 valid nested RARC containers are expanded.

The scan takes **126.332 seconds** on the available Windows host. This is one
end-to-end offline wall-time observation, including original hashing and pack
filename matching. It is not a comparative benchmark, gameplay frame cost or GPU
measurement. Nested compressed containers reuse their already expanded bytes.

## Retained evidence

- [summary.json](summary.json): coverage, exact tool source hashes, catalog digest,
  scan context, validation and limitations.
- [inputs.json](inputs.json): all 3,656 input paths, lengths and original hashes.
- [unresolved.json](unresolved.json): the 178 resources and unresolved texture
  indices/names. These are explicit remaining links, not a target allowlist.
- [tests.log](tests.log): eight focused CTest entries pass, including **47 Python
  cases**: 16 existing identity tests, 12 archive/decoder tests and 19 census tests.

The full metadata catalog remains local at
`build/asset-awareness/catalog.json` rather than adding a 29 MB generated file to
the repository. Its SHA-256 is
`89f39c3861f9dfc803c927ad82dfe7626d00ec8f43604afa15fc8878a1c4aae0`.
No original or replacement payload bytes are committed or distributed.

The Windows package build passes. No rendering behavior changes; the only native
change corrects disclosure of diagnostic capacity exhaustion. A new gameplay
session is not run for this tooling slice. macOS gameplay is unavailable and
support remains best effort; no Metal execution is performed.

The earlier resource-only scan is retained locally as
`build/asset-awareness/catalog-initial.json`. It reports 60 palette-reference
failures and excludes nested contents. The implementation corrects the native
zero-hash handling, adds BMD2 parsing and expands supported nested containers;
the final scan above passes without suppressing malformed-input checks.

## Use and boundary

See [asset awareness](../../asset-awareness.md) for the command and schema.
The source tool is ready for broad metadata-driven work without adding individual
asset identities. Generic runtime draw ownership, animation binding history,
material semantics and live replacement selection remain separate implementation
work. The existing pumpkin renderer experiments do not establish those capabilities
for the full catalog.
