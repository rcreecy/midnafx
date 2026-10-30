# Native player-light response: 2026-10-30

Version 1.18 extends the existing experimental Local light response control to
the default lantern data path. It reads current active player-light values and
native steep attenuation without treating reference distance as an influence
radius. The [feature contract](../../local-lighting.md#player-lantern-source)
records supported modes and approximation limits.

## Artifacts and source semantics

The final local Release package SHA-256 is
`E1C0881A799880C2DDFC8B9E9D5A51A8A0A7A92C149FCF16F187AC1E48A2050D`.
The exact-name native Windows host SHA-256 is
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
The temporary audit package SHA-256 is
`256E606F01C8F27171E02204CA3DC121664DA122EE0A1D861000E7BFB1D8D6F3`.
The [session receipt](native-final/session.json) separates product, host and
fixture; the fixture is not part of the release.

Pinned native source revision `edf42c6a7202647b56dd2fcdef02d17671bc814b`
provides these anchors:

- `src/d/actor/d_a_alink.cpp:14973–14980`: the normal lantern supplies flame
  position, current reference power, color and modes to `dKy_WolfEyeLight_set`.
  `src/d/actor/d_a_alink_HIO_data.inc:1627` and
  `include/d/actor/d_a_alink.h:6358` define default angular OFF, distance STEEP,
  RGB 181/112/40 and full reference distance one.
- `src/d/d_kankyo.cpp:10164–10253`: this setter uses reserved slot zero and
  alpha 255. Room light size can scale reference distance before setting active.
  `:4710` clears activity each environment update; player execution refreshes it
  before world drawing. The renderer copies values after that update.
- `src/d/d_kankyo.cpp:640–679` and `:9012–9018`: native steep response uses
  brightness 0.99999 and `K2=(1-b)/(r*(b*r))`. Stored field 0x14 equal to .99 is
  not that reference brightness. A full reference of one gives half response
  around 316 world units.
- `extern/aurora/lib/gx/shader.cpp:818–821`: the native distance polynomial
  divides angular response. This slice supports only angular OFF and steep
  distance, so it needs no inferred cone direction. Other angular/distance modes,
  special alpha values and boss slots remain excluded.

The separate trace envelope `min(6000,sqrt(99/K2))` reaches one-percent native
response before any cap. Its final quarter fades the added response. Native
per-material room ratios, background offsets and authored normals are not
reproduced; this supplements an already lit LDR image.

## Focused automated proof

The Release ALL build and [five focused checks](focused-tests.log) pass:
`local_lights`, `presets_roundtrip`, `shader_compile`, `ambient_pixels` and
`package_contract`. The selector checks native coefficients, approximately half
response at 316 units, one-percent envelope, reference-power changes, invalid
values, unsupported modes/alpha/activity, capped ranges, rigid transforms and
cleared retired sources. Ordinary zero coefficient payloads preserve their
existing behavior.

The [GPU log](gpu-final.log) has 87 passing cases on Intel UHD Graphics 630,
D3D12: all 79 prior cases plus eight new assertions at 96×80 and odd 65×49.
Independent geometry fixtures establish half and fifth intensity at known
distances, reduced illumination when native power fades, movement of the
illumination footprint with the source, and exact bypass when the source count
is zero. Energy/headroom and alpha bounds remain in force. Source/shader review
finds no blocker. No new setting, preset bit, target or rendering dispatch is added.

## Controlled native integration

The [launcher](launch-native.ps1) runs isolated Ordon `F_SP103,0,27,0` with the
existing exact-name D3D12 host. Local response is on at 15%; grading, AO, shadows,
bounce and other scene effects are off, with motion clarity retained. No external
texture pack or gameplay input is used.

The [audit source](driver.cpp), with [build definition](driver-CMakeLists.txt),
publishes typed default-mode data into inactive reserved slot zero between scene
begin and pre-HUD stages. It does not create/equip a lantern, call the native
setter, or alter inventory. It skips preexisting active owners, snapshots all
previous slot bytes and restores only while the slot still matches its published
bytes. Intervening writers would be preserved. No fixture pointer enters the
product's queued GPU payload.

The [bounded observations](native-final/observations.csv) contain 418 unique
ticks across inactive, moving/power-ramp, inactive-again and full-power phases.
All 150 ramp ticks and 120 recorded final-phase ticks publish active sources.
The 60 middle inactive ticks retain stale nonzero native reference data, yet the
adapter emits no coefficients or envelope. Moving X covers over 100 world units;
reference power covers more than .7. At full reference one the captured native
coefficient is 0.00001001368 and envelope approximately 3144.2766.

The [native log](native-final/native.log) reports the player-light path active
before and after UI reload. Shutdown counters contain 2,176 and 682 balanced
compute/draw callback pairs, with one retained target set each time. Their sum
equals the fixture's 2,858 restored publications, with zero ownership conflicts.
This is aggregate lifecycle agreement, not per-frame GPU readback or timing.
Normal close exits zero and all mods unload. The empty card reports
`Failed to open file: gczelda2`; no shader validation failure is reported.

The [native scene](native-on.jpg) and [existing control](control.jpg) show
integration and updated coverage text. Simulation, animation and particles are
not frozen; these JPEG captures are context, not isolated artistic comparisons.
The [assertion script](analyze.py) produces the [summary](summary.json).

## Limits

Equipped-lantern gameplay, natural equip/put-away transitions, broad scene/motion
quality, unsupported spotlight modes and GPU duration remain unverified. Source
movement and retirement are proven through a controlled channel fixture, not a
native lantern actor route. Existing screen-space occlusion and four-source
selection limits remain. macOS support is best effort through native builds and
portable checks; gameplay and Metal execution are unavailable on this host.
