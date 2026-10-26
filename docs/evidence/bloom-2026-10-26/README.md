# Scene bloom evidence, 2026-10-26

The 1.7.0 slice adds global LDR bloom without asset or stage lists. See
[the feature contract](../../scene-bloom.md) for composition, cost and limits.

## Automated evidence

Eight D3D12 GPU cases pass on the working Windows host. They exercise production
extraction, both filters and grading composition: visible spread from a bright
4×4 patch at the default 20%, red-only retention, dim/black identity, white and
alpha preservation, zero strength, odd-edge averaging, one-pixel targets, the
soft threshold and a fresh black frame without history. Sixteen additional
ordinary/water/detail/AA/LUT combinations pass in `aa_pixels`. Production shader
compilation, preset round trips and the final 1.7.0 package contract pass.
These checks establish bounded synthetic invariants, not whole-game art quality.

## Native observations

The final local package SHA-256 is
`CA9C4E05C923DB7019713CBB1F492D66497C17F8A00366087EB9446A87490FF4`.
The qualified D3D12 baseline host SHA-256 is
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
Native sessions use isolated user data, an empty memory card and autosave off.
Two DDS registrations from `build/asset-hot-reload-pack/A-first` are enabled;
this is not complete 4K-pack verification. Neither texture nor game data ships.

`on.jpg` and `off.jpg` use stage `F_SP103,0,27,0`, a 960×720 target, the same camera
and simulation frozen with `rate 0`. Bloom alone changes at 20%; grading, detail,
adaptive exposure, atmosphere, AO and smoothing are off. AA and enhanced water
remain on. `controls.jpg` records the independent bloom settings. Native water,
cursor and HUD animation can continue with frozen simulation, so JPEG captures
are not lossless parity proof. The native no-controller toast is unchanged.

The effect is subtle in this daylight view. Mean RGB code values for fixed
rectangles are:

| Rectangle (x,y,width,height) | Bloom off | Bloom on |
|---|---:|---:|
| Bright cloth (460,300,65,48) | 140.288 | 141.315 |
| Path (580,260,120,60) | 62.989 | 62.999 |
| Rock (750,360,100,60) | 97.591 | 97.621 |
| Hearts (80,52,90,35) | 88.209 | 88.209 |

These JPEG averages are not linear-light measurements. They support a restrained
native highlight change; the GPU fixture provides precise spread and alpha proof.
They do not prove emissive selectivity or dramatic relighting.

After literal `rate 30`, selecting **Profile / Remaster** enables the combined
effects. F11 changes the target to 1024×768, the module reloads live at normal
simulation rate, then F11 restores 960×720. `resized.jpg`, `reloaded.jpg` and
`restored.jpg` retain these lifecycle observations, not matched comparisons.
`native-final.log` records 4,373 bloom compute callbacks before reload and 865
afterward, with one retained target set at each shutdown. Water logs confirm
the target sizes. Normal close exits zero and logs `all mods unloaded`.
Counts establish dispatch activity, not frame rate or GPU completion duration.

## Failed or unavailable evidence

An initial direct `D_MN04,0,0,0` (Goron Mines) launch stays black without HUD or
visible gameplay. `goron-startup-black.jpg` and `native-goron-startup.log` retain
the failure. No bloom compute shutdown counter is emitted. The console accepts
warp commands, but the session never recovers visible gameplay. One initial warp
was entered with the wrong case/separator before the exact identifier was sent.
The session closes normally with exit zero. No baseline-without-mod control was
run, so the startup cause and attribution remain unresolved. This is not dungeon
qualification and is not proof of a bloom defect or of a successful dungeon.

Both sessions report `Failed to open file: gczelda2` for the isolated empty card.
After `all mods unloaded`, host teardown reports aborted buffer mappings and
`Device lost: Device was destroyed.` No bloom validation error is logged.
Those teardown warnings are retained, not described as a clean warning-free run.
Native macOS gameplay/Metal, whole-game motion and GPU duration are unavailable
or open. macOS builds and portable checks remain in release CI; no hosted Metal
runtime test is added.
