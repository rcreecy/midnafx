# Texture-colored short grass

Ground cover applies material rules across loaded stage backgrounds, including
real-time cutscenes that render those backgrounds. It is enabled for unset
settings and the Remaster profile. Use **Ground cover (grass, gravel, and
litter)** or **Vanilla** to disable it.

Grass, gravel, forest litter, low plants, and moss use separate placement
rules. The pass checks terrain topology and samples supported original CMPR
textures, including external textures loaded by the stage. It skips
unqualified materials, animated secondary backgrounds, actor scenery, and
prerendered footage. This is material-based coverage, with no stage allowlist.

The Windows D3D12 preview showed grass and forest-floor scatter during
Faron's real-time Midna dialogue and preserved the prior spawn counts in
Ordon and Eldin. Adjacent loaded rooms also qualified in Faron and Eldin.
Individual moss and low-plant placements and the full stage inventory remain
visually unverified.

![Ground cover in Faron dialogue](evidence/ground-grass-2026-11-04/faron-dialogue.jpg)

## Ordon baseline

Greener texture samples grow more clumps with shorter blades. Brown
and gray samples retain sparse blades. Stable terrain-space patches vary
clump height and break up density in transitions. Each blade varies in
direction, spread, and height. The meadow pass uses the projected
footprints of three dirt and path materials to exclude grass, with an 18-unit
margin for blade spread. The Windows trial rejected 240,595 candidate roots
against 189 path triangles. It retained 720,000 roots across the room and
counted 16,078 visible clumps in the first sampled frame. Each clump uses
nine blades, for 144,702 blades in that frame. The GPU
expands each clump from one 48-byte record. The frame cap is 18,000 clumps.

The Windows D3D12 capture shows thicker patches and a clear main trail at the
Ordon spawn. Brown ground remains visible in transitions. The trial closed
with exit code zero. GPU duration and path coverage from other views remain
unmeasured.

![Ordon short grass with irregular clumps](evidence/ground-grass-2026-11-03/scatter.jpg)

## Gravel and pebbles

The same ground cover pass adds small stones to Ordon room 0 dirt and path
materials and Eldin Field rooms 7 and 9 gravel material. The mesh follows
the terrain slope, samples color from the original material texture, and
uses ambient light, Twilight tint, and fog. Path material gets a lower
scatter rate than dirt or gravel. Steep triangles and transparent or green
texture samples are excluded. The stones are decorative and add no collision.

Windows D3D12 startup captures counted 44 visible pebbles in Ordon and 735
and 892 in Eldin rooms 7 and 9 respectively. All three trials closed cleanly.
Room transitions and GPU timing were outside this check.

![Pebbles in Eldin Field](evidence/ground-grass-2026-11-03/eldin-gravel.jpg)

## Grass prototype qualification

The first prototype can use the Ordon room 0 terrain model
`F_SP103/R00_00.arc!bmde/model.bmd`. The local asset catalog identifies
`I_kusa` as a 256 × 256 CMPR texture with source hash `6e94ac1fd8d0cb24`.
Material 12, `aa_MA00_kusa_v`, references that texture alongside `I_kumo`.
The model has two signed 16-bit UV streams with 9 and 12 fractional bits.
Material 5 combines grass and dirt, and separate materials use grass-like
textures on walls and vegetation cards. These distinctions give the
prototype a concrete grass surface and useful exclusion cases.

1. Extend the terrain triangle decoder to retain the texture-coordinate
   index at each corner. Confirm which stream and transform feed the
   grass texture in material 12.
2. Scatter short blade clumps deterministically over upward-facing triangles
   of the reviewed material. Sample the grass texture at each root's
   barycentric UV. Use a density mask for blended terrain in a later pass.
3. Render bounded instances after opaque terrain. Use native scene lighting
   and fog, terrain depth for occlusion, and a distance fade for density.
   Keep placement stable as the camera moves. Use original texture identity
   for placement and the resolved image for color when available.

The local replacement inventory has no exact filename candidate for this
Ordon texture. Use the original image for the first proof. Exact color
matching with arbitrary replacement packs needs access to the texture
actually bound by the host, including wildcard or registered replacements.

Compare a fixed route along grass, dirt, and walls. Check root attachment,
edge bleed, shimmer, scene transitions, and GPU duration. The first pass
qualifies one terrain material and one room. Broader coverage can use
reviewed material masks keyed by the existing asset catalog.
