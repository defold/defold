# Font rendering references

These references cover 428 native font rendering cases: 392 existing captures and nine pivot references shared by four configurations. Every case requires at least **98% foreground likeness**, measured over the union of actual and reference foreground pixels using RGB RMSE. Effect visibility, native assertions and independent edge-quality checks must also pass.

The 48 H cases cover scales 0.5, 1 and 2, interior opacity and fractional
outline widths. They compare edge coverage, stroke area and placement against
independent pixel-area coverage in `../../font_coverage.py`.
Original 1.13.1 captures in `1.13.1/` provide an informational comparison;
`1.13.1/sdf-edge-1.13.1-provenance.json` records their source and fixture origin.
The report shows Actual, Reference and Difference, with H images enlarged 4x.

Single-line tests use `ABCDEFGabcdefg 0123456789`. Full-layout English and Arabic paragraphs retain their frozen Lorem ipsum fixtures.

The `alignment/` references cover all nine pivots for issue #13339 using black
`Example` text over a 200×100 white box. They use the editor's native TTF preview
metrics for an offline font (legacy layout), while actual captures use Bob's
compiled glyph bank with its monospaced padding settings. The references were
visually reviewed together; no pixels were shifted or registered to the runtime
output. Comparison uses the fixed white-box region and white background so the
box and outer border do not dilute text-position errors. `alignment/provenance.json`
records generation inputs and PNG hashes. Generate a review candidate from
`engine/font` with:

```sh
./build/arm64-macos/src/test/test_font_bitmap_gen \
  --case compiled_sdf_single_pivot_center --alignment-preview --output build/pivot-preview
```

Replace `center` with `nw`, `n`, `ne`, `w`, `e`, `sw`, `s` or `se` for the other
pivots. This command writes candidates only; ordinary test runs compare compiled
fonts to the frozen references.

`provenance.json` records the checkout revision, dirty-worktree input hashes, executable identities, capture geometry, PNG hashes and the source of retained references. Generated reports include the case options and reproduction command; capture diagnostics are written to the build output. Captures use fixed off-screen targets without resampling or foreground cropping. The current generators use OpenGL; the adapter's driver identity is not recorded by this generator.

The previous patched-1.13.1 reference provenance is retained in `history/pre-current-reset-provenance.json`. Files under `patches/` document that historical generation process; they are not required to generate the current baseline.

From `engine/font`, run:

```sh
cmake --build ../build/arm64-macos --target generate_font_test_images
```

The standalone report is written to `build/font-render-report/index.html`, and captures to `build/font-test-images/`. Ordinary runs never replace accepted references or adjust thresholds. Reference updates require explicit review and acceptance, followed by a fresh capture run.
