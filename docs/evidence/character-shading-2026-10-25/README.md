# Casual Link shading, Windows D3D12

The isolated Ordon run uses stage `F_SP103,0,27,0`, a 960x720 scene target and
the existing external texture pack. AO, atmosphere, grading, rigid smoothing and
camera changes are off; AA and enhanced water are on. The supported original
casual body remains `Bmdl.arc/bl.bmd`. No game assets are retained here.

`casual-on.jpg` and `casual-off.jpg` are matched screenshots with simulation
frozen using the native developer console `rate 0`. Only Enhanced Link shading
changes. The effect changes 297 of 2,282 normal indices. The planner preserves
1,402 indices and both uncertain faces, with zero unresolved conflicts or faces.
Absolute RGB differences in window coordinates `[395,250,553,580)` average
0.314 levels and reach 17 levels. Exactly 1,469 pixels exceed three levels in
any channel. All such pixels fall inside that character rectangle. The sampled
scenery rectangle `[240,160,400,240)` is identical. These screenshots show a subtle
lighting-continuity change; they do not establish broader artistic quality.

The comparison package SHA-256 is
`3C1725ECB1A24A70A41774BDFB9FA0705A6ACC7F7D74BDB88C756A56C031341C`.
The native excerpt records initial apply, direct disable restoration, re-enable
after simulation resumes, native module reload with cached rebind and normal
shutdown. All three restorations copy 13,692 bytes exactly, with original FNV-1a
`3f85e23fd6ac3e78`. The owned host exits with code zero. Native developer `rate 0`
delays activation until another game update; restoration runs from the control.

The final v1.4.0 package additionally contains the reviewed allocation-failure
guard around cached analysis. Its separate native startup/shutdown smoke also
applies 297 changes, restores the same 13,692 bytes exactly and exits zero.
Final local package SHA-256:
`F83F45A47BA31AB04B8A669E4E084A12E1B4A14C362EF4A4569452357DB4BBEE`.
See `native-comparison.log` and `native-final.log` for bounded log excerpts.

Both runs use the qualified baseline host SHA-256
`444D8EE33ED78BA4A427810E35A62731F4D845CBAC7343B37B16A5C20F3C34A4`.
Only the existing missing `gczelda2` card-file error appears on stderr.
All 22 Windows CTest entries pass before the final metadata/exception guard
change; the four affected identity, smoothing, profile and package checks pass
again after the final build. No Metal runtime test is performed.
