# Richer Natural palette

The November 11 revision strengthens MidnaFX Natural toward the requested vivid
remaster direction. It adjusts the existing 33-cube color lattice rather than
adding a new rendering pass or another user control. Natural remains the default
look, and Remaster continues to select it. Saved look selection and intensity
still apply. Neutral and the Twilight-specific lattice retain their previous
definitions.

Muted midtone colors receive the largest chroma increase. The response fades in
deep shadows and bright highlights, reduces the increase for already saturated
colors, and gives warm red/orange regions a smaller boost. Green/yellow-green
regions get additional separation. A mild luminance curve deepens lower midtones
and lifts upper midtones. Out-of-gamut colors compress toward luminance instead
of independently clipping channels.

This is an art-directed color transform in the existing scene-code color space.
It is not semantic material or skin recognition, scene relighting, HDR output,
or a reconstruction of the reference game's renderer. It cannot remove baked
lighting or recover color distinctions missing from the original textures.

The runtime still samples the same cached LUT through its existing grading,
AA/detail and enhanced-water paths. There are no added per-pixel texture reads
or shader instructions. The CPU performs additional arithmetic when constructing
the lattice. The existing look intensity and bypass controls provide adjustment.

Native Ordon and Faron Spring daylight observations and focused validation are
recorded in the [palette receipt](evidence/vivid-palette-2026-11-11/README.md).
Whole-game artistic consistency and macOS gameplay remain unverified.
