# Sampled replacement registration identity

Follow-up: [registry conflict qualification](asset-replacement-conflicts.md) tests priority, equal-priority ordering, wildcard fallback, clearing and stale registration IDs in the native Aurora harness. Live conflicting-pack draws remain a separate gate.

The experimental host now carries the replacement registry's monotonically assigned ID on each newly created texture handle. The slot-0 bind-group snapshot copies that value into the native draw command. A matching `AssetProvider` record supplies the actual file entry's basename and an FNV-1a fingerprint of its path. Only the two known pumpkin DDS basenames are logged; raw and virtual providers remain unqualified.

This extends [texture binding evidence](asset-texture-binding.md). It distinguishes an actual bound registration from a candidate filename found offline. The ID identifies a registration within one host process; it is not a persistent asset ID, content hash, provider generation, or cross-session key. Thumbnail and full-size handles can retain the same registration ID. A created handle alone does not prove selection: the offline join requires a sampled encoded draw carrying that registration.

## Ownership and validation

Provenance is assigned before a handle is published and remains immutable. Frame commands contain copied integer values, with no registry or file pointer on the render worker. Old in-flight draws retain their old registration even if a later registration wins. Existing registry selection, priority, cache invalidation and streaming behavior are unchanged.

The parser accepts repeated identical provider records, rejects conflicting metadata for an ID, joins independently of log order, and validates the provider filename against the original material's slot-0 texture key. Missing provider metadata remains unobserved. Parse one host process log at a time; combining logs can collide registration IDs. The path fingerprint disambiguates observed path strings but is non-cryptographic and does not attest file bytes. It is not used as the original asset identity.

The file observer is bounded at 4,096 creation observations. It can report a handle that is later discarded; such a record establishes no selection without matching draw evidence. The host patch still runs its narrow file observer even when mod draw labels are off. Mod diagnostics default to off, and ordinary package builds do not apply the experimental patch.

## Live evidence and validation

The [summary](evidence/asset-provider-2026-10-20/summary.json) records host, patch, unchanged mod package and full-log hashes. Metadata-only graphs and events are committed. Full logs, isolated settings and screenshots remain under `build/m13-aa-runtime/asset-provider-*`.

- [Vanilla l](evidence/asset-provider-2026-10-20/pumpkin-graph-l.json): 32 encoded draws match 32 labels; both original 64×64 bindings report registration zero.
- [Pack m](evidence/asset-provider-2026-10-20/pumpkin-graph-m.json): 64 encoded draws match 64 labels over two Ordon loads. Material 0 binds registration 148 from `tex1_64x64_630d5c7bc606585d_14.dds`; material 1 binds registration 146 from `tex1_64x64_000e342106676e31_14.dds`. Both retain those registrations from initial 64×64 thumbnails through full-size 2048×2048/1024×1024 bindings after the scene round trip. One archive deletion is observed.
- [Disabled n](evidence/asset-provider-2026-10-20/pumpkin-graph-n.json): pack and identity diagnostics remain enabled, but draw diagnostics are disabled. Handle creation records still occur; zero labels and encoded draw records establish no sampled provider binding.

All three sessions render and shut down normally. The experimental Windows D3D12 host builds successfully. Windows CTest passes **18/18** in 37.81 seconds. Fourteen Python cases cover legacy logs, provider conflicts, missing metadata, source matching and recorded observations. The helper confirms reverse-applicability at the pinned Aurora revision. No game or replacement texture payload is committed.

Hosted [CI run 39523310202](https://github.com/rcreecy/midnafx/actions/runs/39523310202) passes at implementation commit `3001f81`: Windows **18/18** in 34.86 seconds and macOS Intel **18/18** in 28.34 seconds. Three Metal AA repeats pass in 14.99, 14.82 and 13.08 seconds. Both ordinary native packages are produced; the release job is skipped. CI does not compile the experimental Aurora host patch.

## Cost and remaining scope

The extension adds one 64-bit provenance field to each host texture reference and one to the bind-group snapshot. It remains within the experimental 160-byte payload capacity established previously; aligned structure costs may exceed field sizes. Complete CPU/allocation overhead, conflicting pack priority scenarios, registration removal/re-addition, virtual providers, native Metal-host builds, GPU completion and full file-content attestation remain unverified. No rendering effect or release default changes.
