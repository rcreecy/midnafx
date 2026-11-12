# Camera-view grass — November 11, 2026

Windows D3D12, original textures, default settings. Hyrule Field
`F_SP121,6,2,4`, native time 180. The native console then moves the camera with
`camera tp -37006 -6520 97964 28500 -3200`.

[Detached-camera capture](field.png).

The activation receipt reports 38,365 grass roots: 16,618 near, 18,420 middle,
3,327 outer, with a view range of 149,043 world units. The fixed 10,000-unit
limit now belongs to the middle layer; the new outer layer extends beyond it.
Both the gameplay and detached-camera views render normally. Normal close
returns exit code 0. No WGSL/GPU validation errors are logged. The isolated clean
trial also logs the existing missing `gczelda2` card file.

Camera math tests cover far-view diagonal extent, terrain range limits, bounded
adaptive tile sizes, invalid camera input fallback and conservative view-box
culling. The camera math and final package contract tests both pass.

Package SHA256:
`7870C9142FCAEDD54EAF9ACF3EBF2E429D562FAE765E556AAF922581A52B9748`.
Host SHA256:
`FDBDD3F535351732538BC379A7E75BCCD6707D2F3C8BEF744C680EC3DD11BEA3`.

The capture is unfrozen, not an isolated before/after comparison or proof of
complete full-resolution coverage. Distant grass uses enlarged, coarse clumps;
material qualification and cache/draw budgets still apply. High-speed travel,
whole-game artistic coverage, GPU cost, 4K and macOS gameplay remain unverified.
