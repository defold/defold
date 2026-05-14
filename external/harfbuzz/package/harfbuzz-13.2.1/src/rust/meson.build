rust = import('unstable-rust')

hb_rs = rust.bindgen(
  input : '../hb.h',
  output : 'hb.rs',
  include_directories: incsrc,
  args : ['--allowlist-function=hb_.*',
          '--allowlist-type=hb_.*',
          '--no-copy=hb_.*',
          ],
)

cargo = find_program('cargo')
rustfmt = find_program('rustfmt')

rust_flags = ''
cargo_args = [
  '--package', 'harfbuzz_rust',
  '--lib',
  '--target-dir', meson.current_build_dir(),
  '--manifest-path', meson.current_source_dir() / 'Cargo.toml',
]

features = []
if conf.get('HAVE_FONTATIONS', 0) == 1
  features += ['font']
endif
if conf.get('HAVE_HARFRUST', 0) == 1
  features += ['shape']
endif
if features.length() > 0
  cargo_args += ['--features', ','.join(features)]
endif

buildtype = get_option('buildtype')
if buildtype == 'release' or buildtype == 'debugoptimized'
  cargo_args += [
    '-Z', 'build-std=std,panic_abort',
    '-Z', 'build-std-features=optimize_for_size',
  ]
  cargo_args += ['--profile', buildtype]
endif

opt_level = get_option('optimization')
rust_flags += ' -C force-frame-pointers=yes -C force-unwind-tables=no -C opt-level=' + opt_level

sources = [
  'lib.rs',
  'font.rs',
  'shape.rs',
]

harfbuzz_rust = custom_target(
  'harfbuzz_rust',
  input: sources + ['Cargo.toml'],
  output: ['libharfbuzz_rust.a'],
  depends: [hb_rs],
  env: ['OUT_DIR=' + meson.current_build_dir(),
        'RUSTFLAGS=' + rust_flags,
  ],
  command: [
    cargo, 'build',
    ] + cargo_args + [
    '-Z', 'unstable-options',
    '--artifact-dir', meson.current_build_dir(),
  ],
  install: true,
  install_dir: join_paths(get_option('prefix'), 'lib'),
)

harfbuzz_rust_dep = declare_dependency(
  link_with: harfbuzz_rust,
)

clippy_fix = run_target(
  'clippy-fix',
  env: ['OUT_DIR=' + meson.current_build_dir()],
  depends: [hb_rs, harfbuzz_rust],
  command: [
    cargo, 'clippy',
    ] + cargo_args + [
    '--allow-dirty', '--fix',
  ],
)
if get_option('tests').enabled() and cargo.found()
  test(
    'clippy',
    cargo,
    env: ['OUT_DIR=' + meson.current_build_dir()],
    depends: [hb_rs, harfbuzz_rust],
    args: [
      'clippy',
      ] + cargo_args + [
      '--', '-D', 'warnings',
    ],
    timeout: 300,
  )
endif

# Convert source files in their src dir by transforming the `sources` list.
sources_in_source_dir = []
foreach s : sources
  sources_in_source_dir += meson.current_source_dir() / s
endforeach

rustfmt_fix = run_target(
  'rustfmt-fix',
  env: ['OUT_DIR=' + meson.current_build_dir()],
  depends: [hb_rs],
  command: [
    rustfmt,
    '--edition', '2021',
    '--',
    sources_in_source_dir,
  ],
)
if get_option('tests').enabled() and rustfmt.found()
  test(
    'rustfmt',
    rustfmt,
    env: ['OUT_DIR=' + meson.current_build_dir()],
    depends: [hb_rs],
    args: [
      '--check',
      '--edition', '2021',
      '--',
      sources_in_source_dir,
    ],
  )
endif
