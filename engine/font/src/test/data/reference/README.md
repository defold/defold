# Font rendering references

These 344 backend-independent reference images represent the user-approved current rendering upgrade. Every case requires at least **98% foreground likeness**, measured over the union of actual and reference foreground pixels using RGB RMSE. Effect visibility and native assertions must also pass.

Single-line tests use `ABCDEFGabcdefg 0123456789`. Full-layout English and Arabic paragraphs retain their frozen Lorem ipsum fixtures.

`provenance.json` records the checkout revision, dirty-worktree input hashes, executable identities, capture geometry, PNG hashes and repeat verification. Generated reports include the case options and reproduction command; capture diagnostics are written to the build output. Captures use fixed off-screen targets without resampling or foreground cropping. The current generators use OpenGL; the adapter's driver identity is not recorded by this generator.

The previous patched-1.13.1 reference provenance is retained in `history/pre-current-reset-provenance.json`. Files under `patches/` document that historical generation process; they are not required to generate the current baseline.

From `engine/font`, run:

```sh
cmake --build ../build/arm64-macos --target generate_font_test_images
```

The standalone report is written to `build/font-render-report/index.html`, and captures to `build/font-test-images/`. Ordinary runs never replace accepted references or adjust thresholds. Reference updates require explicit review and acceptance, followed by a fresh capture run.
