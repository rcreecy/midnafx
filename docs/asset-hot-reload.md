# Live replacement reload on an existing pumpkin resource

October 21 correction: [Windows launch identity](asset-host-name.md) explains missing discovery in the later overhead trials and invalidates their CPU/memory comparison. Earlier reload evidence here uses the original `dusklight.exe` name and is unaffected. Full observer overhead remains open; standalone native layout measurements remain valid.

The Windows D3D12 session switches from the initial replacement provider to another provider, then to original textures, then back to the initial provider with fresh registrations. The host keeps rendering the same scene throughout. The pumpkin archive loads once, records 12 creations, and is never deleted during the session. No scene warp, mod reload or host restart occurs between the sampled phases.

## Bounded resampling

The developer panel adds **Sample pumpkin draws again** beside the existing identity and draw diagnostic toggles. The button queues one coalesced atomic request only when both toggles are enabled. The next outer shape callback scans the existing bounded metadata registry for the exact loaded pumpkin fingerprint, restores its two material lookup keys, and permits 32 further samples. It does not rescan archives or retain an instance pointer. A request without an observed loaded pumpkin produces no samples.

Labels retain the module epoch and increasing token across requests. A `draw-resample` event records the next token. The existing 4,096-event ceiling still applies. Reset clears pending requests. Disabled callbacks retain their early return; enabled callbacks add an atomic flag load, with an exchange only when a request is pending. The only metadata scan occurs in response to a request.

## Live result

In isolated session `asset-hot-reload-r`, the replacement root initially contains copied, verified pumpkin files under `A-first` and `z-second`. Files are moved only within the experiment's build workspace. Native **Enable Texture Replacements** is switched off and on after each file change, invoking the existing registry reload callback. The game scene continues rendering behind the native graphics tuner and mod panels.

| Tokens | Change | Sampled slot-0 result |
|---|---|---|
| 1–32 | Initial load | `A-first`; registrations 1/2; thumbnails and full-size replacements |
| 33–64 | Remove `A-first`, reload, request samples | `z-second`; registrations 8/9; 1024×1024 pumpkin and 2048×2048 leaves |
| 65–96 | Remove `z-second`, reload, request samples | Original 64×64 textures; registration zero |
| 97–128 | Re-add `A-first`, reload, request samples | Restored full-size replacements; fresh registrations 10/11 |

Each phase encodes **32/32** matching sampled draws and observes both materials. The restored file paths match the initial path fingerprints, while their registration IDs differ. This demonstrates that provider path identity and registration lifetime are separate relationships. IDs need not be contiguous because other host replacements can also register.

After phase four, draw diagnostics are disabled and the button is pressed once more. No fourth resample event or additional labels occur. The final configuration confirms draw labels are off. The host shuts down normally. No source game or pack file, installed user setting, visual effect, or production default changes.

## Evidence and validation

[Summary](evidence/asset-hot-reload-2026-10-20/summary.json), [joined graph](evidence/asset-hot-reload-2026-10-20/pumpkin-graph-r.json), and [raw metadata events](evidence/asset-hot-reload-2026-10-20/asset-hot-reload-r.jsonl) are committed. They include the package/host/log hashes, one-load lifecycle counts, resample boundaries and phase-specific bindings. Screenshots, full logs and isolated configuration remain under `build/m13-aa-runtime/asset-hot-reload-r`.

The tested package SHA-256 is `15ec60f960a1277f4441fd26358ef13196534018b8be29ab77e25bb6ab9dbcb3`. The experimental host is unchanged from the [provider experiment](asset-provider-correlation.md). The Windows Release package builds successfully. The Python suite passes **16/16**, including reconstruction of the binding graph from raw events and checks for all four phases, unique labels, unchanged resource lifetime and fresh registrations.

Full local Windows CTest passes **18/18** in 42.67 seconds. Whitespace checks also pass. Native macOS gameplay remains untested.

Hosted [CI run 39539957849](https://github.com/rcreecy/midnafx/actions/runs/39539957849) passes at implementation commit `e43f68e`: Windows **18/18** in 33.47 seconds and macOS Intel **18/18** in 28.09 seconds. Three Metal AA repeats pass in 12.26, 14.53 and 15.79 seconds. Both ordinary native packages are produced; the release job is skipped. CI builds the new mod control and checks recorded metadata, but does not exercise native macOS gameplay or compile the experimental host patch.

The mod observer reports cumulative 111.1358 ms across 2,061,519 enabled shape callbacks. This measures its pre/post bodies, excludes native draw execution, and does not measure complete host metadata/logging/allocation costs. It is not a controlled overhead benchmark.

## Reproduction and limits

Start the pinned diagnostic host with an isolated user/mod directory, both asset diagnostics enabled, and Ordon `F_SP103,0,27,0`. Use verified copies of the two known pumpkin DDS files in the duplicate directories. Follow the table's directory changes without leaving the scene. After each change, use native Settings → Video → Enable Texture Replacements, off then on; then open Mods → MidnaFX → Developer: model qualification and press the sample button. Close the host before exporting redirected logs.

This proves observed hot reload and subsequent binding correctness for this scene and these files. GPU completion and the exact overlap of old frames with registry mutations were not instrumented. It does not prove every in-flight race, device/backend, virtual provider, mod load order, or whole-game compatibility. Total observer overhead and experimental Metal-host qualification remain open.
