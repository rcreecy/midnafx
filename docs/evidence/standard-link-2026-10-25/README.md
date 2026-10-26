# Standard outfit visual comparison

These images retain the initial standard-outfit shading comparison, before the
subsequent re-enable repair. The candidate package SHA-256 is
`FD466B619CB104FB1E9F6E6CB90D6709605D4A244B11C44BE5CBAF97917BF16B`.
They are not captures of the final release package.

`standard-on.jpg` and `standard-off.jpg` use the same paused Ordon scene and pose
at a 960x720 scene resolution with external replacements enabled. The native
developer console freezes simulation with `rate 0`. Only Link shading changes
between the images; the cursor and its glow also move outside the character.

In window rectangle `[395,240,555,590)`, the absolute RGB difference averages
0.176 levels and reaches 11 levels. There are 814 pixels above three levels in
any channel in that rectangle. Sampled scenery `[240,160,400,240)` is identical.
The full image is not identical outside the character because of the cursor/glow.
This is a subtle shading-continuity change, not additional geometry or a complete
character art review.

The initial candidate changed 194 normals and preserved 1,044 conflicting indices,
but later re-enable failed after a native draw copy changed. The
[final repair and native evidence](../standard-link-2026-10-26/README.md) supersede
that candidate's lifecycle behavior.
