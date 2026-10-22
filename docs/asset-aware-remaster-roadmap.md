# Asset-aware remaster roadmap

October 21 correction: [renamed Windows host loading](asset-host-name.md) explains missing discovery and invalidates the nine-run CPU/memory comparison. The repaired launcher preserves `dusklight.exe` and verifies one host image. The standalone 56-byte command-size increase remains valid. Repeat overhead measurements with populated records before extending rendering behavior; timing/allocation and experimental Metal qualification remain open.

Directory compatibility update: [live duplicate-file evidence](asset-live-directory-conflicts.md) confirms first-sorted-file selection within one directory load and fallback after provider removal between launches. This is distinct from priority among separate registry entries.

October 20 update: the [native draw experiment](asset-draw-correlation.md) proves both pumpkin materials reach WebGPU encoding in vanilla and pack-enabled Windows sessions. The original priorities below remain useful; the submission-identity gap is now narrowed to generalization, resolved replacement selection, complete overhead measurement, and experimental Metal-host validation.

The pumpkin proof establishes original resource, material, texture and shared creation relationships, including preservation with the tested Henriko pack. It does not establish an actor census, visibility, native draw attribution or material shader control. See [validation](asset-intelligence-validation.md) and [the source audit](asset-intelligence-investigation.md).

## Priorities

| Capability | Required metadata and rendering control | Existing support / missing interface | Cost, risk and evidence |
|---|---|---|---|
| 1. Replacement compatibility diagnostics | Original source key plus resolved replacement provider/generation/status | Original keys and real candidate filenames proven; add read-only TextureService selection query | Low-to-medium implementation cost; low visual risk; strong identity evidence. Useful immediately for pack conflicts. |
| 2. Resource/material profiles | Stable model fingerprint and material index; draw filtering before applying a profile | Identity graph proven; native submission token/observer missing | Medium cost; requires bounded game-thread to render-worker metadata transfer. Strong lookup evidence, shading control unproven. |
| 3. Selective geometry replacement | Fingerprinted BMD, topology, skeleton, ownership and instance constraints | Existing geometry rebuild/smoothing infrastructure; wider asset qualification required | Medium-to-high per-asset cost, memory/load-time impact, medium compatibility risk. Existing narrow geometry evidence is stronger than generic PBR evidence. |
| 4. Environment-specific atmosphere | Stage/room context, depth and scene profile | Existing game state and screen-space passes; no material identity required for a coarse profile | Low-to-medium cost and good achievable visual benefit. Fine object exclusion still needs masks/draw attribution. |
| 5. Asset-aware color grading | Material/instance mask and stable profile; selective composition | Current grading/LUT passes work on the scene, not individual identified objects | Medium cost plus mask pass/bandwidth; halos and transparency ordering risks. Identity lookup is proven, masks are not. |
| 6. Roughness/specular profiles | Semantic material parameters, normals, lights, view vectors and shader control | Existing selective water response offers a narrow example; no generic material parameter service | High cost/risk. TEX1 names and hashes do not reveal physically meaningful roughness. Hand-authored profiles and draw control required. |
| 7. Selective reflections | Material mask, normals, depth and reflection inputs; per-material blend control | Water-specific reflection path exists; generic identity-to-reflection binding missing | High GPU/bandwidth cost, temporal/disocclusion risk. Do not generalize water results to opaque models. |
| 8. Normal maps | Tangent basis, UV mapping, replacement normal texture convention and shader injection | Source texture key can anchor authored metadata; tangent/shader/normal-map service missing | High implementation/content cost; UV seams and handedness need per-asset proof. No normal-map rendering evidence in this PoC. |
| 9. Material-aware lighting | Material semantics, normals, lights/shadows and native shading pipeline control | Model/material lookup available; generic lighting/shader interfaces missing | Very high scope and compatibility risk. Requires a separate renderer design and performance budget. |

High-resolution pack support is a compatibility constraint across every priority. Preserve original identity as the key, store replacement identity separately, respect mod priority, and never assume one image hash identifies one material. The tested pack uses higher-resolution DDS payloads with original 64×64 dimensions in the lookup filenames. Replacement load/eviction and animated texture changes must not corrupt persistent material profiles.

## Recommended next milestone: filtered submission proof

Add a small optional host capability that observes only the fingerprinted pumpkin's J3D packets and carries copied model/material/shape tokens to the corresponding native draw. Add a read-only resolved replacement query if the same experiment needs selected image attribution. Keep both diagnostics disabled by default. No material shader or effect should change during this milestone.

The host design must specify game-thread capture, render-worker payload ownership, archive/frame token invalidation, mod reload cleanup, capacity limits and unsupported-version behavior. A token cannot be a persistent raw pointer. Export original texture keys alongside selected replacement generation/provider, and preserve multiple instances and shared textures as separate relationships.

Acceptance: both pumpkin materials correlate to actual submissions in vanilla and the tested pack; scene round trip and mod reload invalidate old tokens; disabled mode adds no scanning; bounded timing/allocation measurements cover the full observer; Windows D3D12 and hosted Metal compilation pass. Native macOS gameplay remains a separate gate until hardware is available. Stop at this proof before proposing a material-specific visual change.

This milestone has the strongest evidence-to-cost ratio: it closes the exact gap between the proven identity graph and future rendering controls. Broad PBR, automatic material classification and a renderer fork remain outside scope.
