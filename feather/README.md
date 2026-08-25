# Feather Harness

Small standalone macOS/Metal harness for iterating on feathering without the
full Defold text pipeline.

Current scope:
- offscreen Metal render
- optional interactive Metal app mode
- one hardcoded L-shape
- analytic feathering in the fragment shader
- PNG output for deterministic inspection

Build:

```sh
cd feather
./build.sh
```

Run:

```sh
./build/feather_harness --output build/l_shape.png
```

Interactive mode:

```sh
./build/feather_harness --interactive --width 768 --height 768 --feather 28 --output build/l_shape.png
```

Useful flags:

```sh
./build/feather_harness --output build/l_shape.png --width 512 --height 512 --feather 28
./build/feather_harness --interactive --output build/l_shape.png --feather 28
./build/feather_harness --help
```

Interactive controls:
- Arrow keys: change feather amount
- Mouse drag horizontally: change feather amount
- `S`: save a PNG with a unique iteration suffix

The current feather amount is shown in the top-right corner of the window.

Notes:
- This harness is intentionally isolated from font loading, text layout, and
  Defold material/shader translation.
- The first shader pass uses a simple Rive-inspired analytic feather function
  for one known problematic corner shape. It is a testbed, not a finished port.
