# Pumpkin native draw correlation

The [manual resample control](asset-hot-reload.md) extends the original 32-sample lifetime budget with explicit bounded requests. Labels continue increasing; discovery still observes new loads only unless a known resource is explicitly resampled.

Follow-up: [sampled texture binding](asset-texture-binding.md) now records actual slot-0 replacement status and dimensions, including thumbnail-to-full-size observations. Exact provider/file selection remains unobserved.

## Result and scope

The isolated Windows D3D12 experiment connects both original pumpkin materials to native WebGPU draw encoding, with vanilla textures and the user's Henriko pack. This extends the [original identity proof](asset-intelligence-validation.md). It does not prove GPU completion, visibility after depth testing, resolved replacement selection, or generic material shader control.

The developer setting `asset_draw_diagnostics` is disabled by default and requires `asset_diagnostics`. Labels alone are explicitly insufficient evidence. An unpatched host cannot produce the matching `AssetDraw` records used by the offline join.

## Implementation

The mod brackets `J3DShape::drawFast` with exported GX debug groups. It filters the exact original fingerprint `pumpkin.arc/pumpkin.bmd#fnv1a64:9b2ddb5ecbd95421`, validates the material against the loaded model, and samples at most 32 shape calls per archive lifetime. Cached material addresses are session-only lookup keys; archive deletion and mod reset clear them. Copied labels contain a module epoch and increasing token, never an address. No instance pointer is retained. Eight nested scopes are supported; deeper calls remain unlabelled and preserve scope balance.

The opt-in [Aurora patch](../patches/aurora-asset-draw-trace.patch) retains copied debug groups in frame packets, prevents draw batching across traced scope boundaries, and reports only after successful pipeline binding and an actual `Draw` or `DrawIndexed` call. The render worker borrows a label only during synchronous command encoding. Its log counter saturates at 4,096 records. This is an experimental host modification, not part of the production host or automatic package build.

The [patch helper](../tools/asset-draw-host-patch.py) requires Aurora commit `7f2801cd0133c9333eadb4e2e6b24100c328d328`. It checks forward and reverse applicability, supports explicit `--apply` and `--reverse`, and refuses conflicting edits. Dusklight remains pinned to `edf42c6a7202647b56dd2fcdef02d17671bc814b`.

The offline join accepts worker records arriving before their corresponding labels. Only validated matching tokens establish encoding. Missing host records remain unencoded; unknown labels do not establish identity. Original material and texture keys remain unchanged.

## Live evidence

[Summary and artifact hashes](evidence/asset-draw-2026-10-20/summary.json) and metadata-only event subsets are committed. Game archives, texture payloads, screenshots and full logs remain local.

| Session | Labels | Encoded draws | Materials | Result |
|---|---:|---:|---|---|
| [f: vanilla round trip](evidence/asset-draw-2026-10-20/pumpkin-graph-f.json) | 64 | 64 | 0, 1 | Two resource loads, one archive deletion, 24 creations; native mod reload and normal shutdown |
| [g: Henriko enabled](evidence/asset-draw-2026-10-20/pumpkin-graph-g.json) | 32 | 32 | 0, 1 | Original identity unchanged; 12 creations; normal shutdown |
| [h: draw diagnostics disabled](evidence/asset-draw-2026-10-20/pumpkin-graph-h.json) | 0 | 0 | Unobserved | Identity diagnostics still active; normal shutdown |

The final flushed logs match every sampled label. An earlier read while session f was running temporarily lacked two host records; it was incomplete evidence, not a proven missing draw. After native mod reload, 28,861 callbacks emitted no new labels because existing resources are not retrospectively rescanned. New archive loads receive new tokens; outstanding frame commands own their copied strings.

All three sessions use package SHA-256 `7443ef6468082f48ee3311aadb490518e54ecafbe72da20bb2398338667a9287`. The tested working tree is based on `b3f691f`; the summary records the exact host and patch hashes used at capture. A later counter hardening changes increment placement to prevent unsigned wrap after saturation; it does not affect these sub-limit observations.

Local evidence is under `build/m13-aa-runtime/asset-draw-vanilla-f`, `asset-draw-pack-g`, and `asset-draw-disabled-h`. The vanilla capture is `ordon-return.jpg`; the other captures are `ordon.jpg`. Scene context is Ordon `F_SP103,0,27,0`; the round trip uses `warp F_SP127 0 0 0`, then `warp F_SP103 27 0 0`.

## Cost and limitations

The mod measures pre/post observer bodies, excluding native draw execution. Vanilla before reload records 32.8543 ms over 518,662 callbacks; after reload, 1.0095 ms over 28,861 callbacks. Pack-enabled observation records 2.6593 ms over 68,577 callbacks. These are cumulative observations, not controlled benchmarks or per-frame guarantees.

Disabled callbacks return before timers, scans, allocations and event logging. Hook dispatch and setting checks still occur. The experimental host retains debug metadata even when the mod switch is off, and traced boundaries can split batches. Full host CPU/allocation overhead, deterministic pixel equality, GPU completion, and native macOS gameplay remain **NOT TESTED**. The broader milestone's complete overhead and experimental Metal-host gates therefore remain open.

The existing resource discovery cap can prevent discovery of later assets. This proof covers the observed pumpkin only. Material animation, multiple simultaneous pumpkin archives, all game scenes and other assets are not qualified. No shading, geometry, release version, or production defaults change.

## Automated verification

The Windows Release package build and isolated experimental host build pass. Windows CTest passes **18/18** in **37.62 seconds**. The final Python asset suite passes **10/10**, including recorded vanilla/pack/disabled joins and malformed host records. Patch reverse-applicability and whitespace checks pass. These tests do not substitute for the untested full-overhead and Metal-host gates.

Hosted [CI run 39434827154](https://github.com/rcreecy/midnafx/actions/runs/39434827154) passes at implementation commit `76366b3`: Windows **18/18** in 43.07 seconds and macOS Intel **18/18** in 17.92 seconds. Three additional Metal AA pixel runs pass in 8.81, 9.45 and 8.80 seconds. Both native packages are produced; the release job is skipped. CI builds the mod against the ordinary pinned SDK, not the experimental Aurora host patch.

## Reproduction

Apply the patch with `python tools/asset-draw-host-patch.py --apply`, then rebuild the isolated host. Enable both asset settings in an isolated user directory before loading Ordon. Repeat with the existing pack, then with draw diagnostics disabled. Close the host before parsing redirected output.

Run `tools/asset-intelligence.py` with the original pumpkin archive, `--runtime-log`, `--midnafx-revision`, `--platform windows-amd64`, `--backend d3d12`, and `--scene F_SP103`; supply `--replacement-dir` for filename candidates. Candidate files still do not attest which image a native draw selects.

Next work is a read-only resolved replacement query and complete observer profiling, followed by an experimental Metal host build. Stop before material-specific visual changes.
