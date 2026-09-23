# Font rendering references

These 392 reference images cover native font rendering. Every case requires at least **98% foreground likeness**, measured over the union of actual and reference foreground pixels using RGB RMSE. Effect visibility, native assertions and independent edge-quality checks must also pass.

The 48 H cases cover scales 0.5, 1 and 2, interior opacity and fractional
outline widths. They compare edge coverage, stroke area and placement against
independent pixel-area coverage in `../../font_coverage.py`.
Original 1.13.1 captures in `1.13.1/` provide an informational comparison;
`1.13.1/sdf-edge-1.13.1-provenance.json` records their source and fixture origin.
The report shows Actual, Reference and Difference, with H images enlarged 4x.

Single-line tests use `ABCDEFGabcdefg 0123456789`. Full-layout English and Arabic paragraphs retain their frozen Lorem ipsum fixtures.

`provenance.json` records the checkout revision, dirty-worktree input hashes, executable identities, capture geometry, PNG hashes and the source of retained references. Generated reports include the case options and reproduction command; capture diagnostics are written to the build output. Captures use fixed off-screen targets without resampling or foreground cropping. The current generators use OpenGL; the adapter's driver identity is not recorded by this generator.

The previous patched-1.13.1 reference provenance is retained in `history/pre-current-reset-provenance.json`. Files under `patches/` document that historical generation process; they are not required to generate the current baseline.

From `engine/font`, run:

```sh
cmake --build ../build/arm64-macos --target generate_font_test_images
```

The standalone report is written to `build/font-render-report/index.html`, and captures to `build/font-test-images/`. Ordinary runs never replace accepted references or adjust thresholds. Reference updates require explicit review and acceptance, followed by a fresh capture run.
