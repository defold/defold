project('harfbuzz', ['c', 'cpp'],
  meson_version: '>= 0.60.0',
  version: '11.3.2',
  default_options: [
    'cpp_eh=none',          # Just to support msvc, we are passing -fno-exceptions also anyway
    # 'cpp_rtti=false',     # Do NOT enable, wraps inherit it and ICU needs RTTI
    'cpp_std=c++11',
    'wrap_mode=nofallback', # Use --wrap-mode=default to revert, https://github.com/harfbuzz/harfbuzz/pull/2548
    'buildtype=debugoptimized',
  ],
)

glib_min_version = '>= 2.30.0'
cairo_min_version = '>= 1.10.0'
chafa_min_version = '>= 1.6.0'
icu_min_version = '>= 49.0'
graphite2_min_version = '>= 1.2.0'

freetype_min_version_actual = '>= 2.4.2'
freetype_min_version = '>= 12.0.6'    # Corresponds to `freetype_min_version_actual`

hb_version_arr = meson.project_version().split('.')
hb_version_major = hb_version_arr[0].to_int()
hb_version_minor = hb_version_arr[1].to_int()
hb_version_micro = hb_version_arr[2].to_int()

# libtool versioning
hb_version_int = 60000 + hb_version_major*100 + hb_version_minor*10 + hb_version_micro
hb_libtool_version_info = '@0@:0:@0@'.format(hb_version_int)

pkgmod = import('pkgconfig')
cpp = meson.get_compiler('cpp')
null_dep = dependency('', required: false)


# Includes Microsoft Clang compiler with GNU arguments, see
# https://github.com/harfbuzz/harfbuzz/pull/4394
cpp_is_microsoft_compiler = host_machine.system() == 'windows' and cpp.get_define('_MSC_FULL_VER') != ''

# Only perform these checks if cpp_std is c++11 as setting -std directly
# produces a warning from meson.
if get_option('cpp_std') == 'c++11'
  # Enforce C++14 requirement for MSVC STL
  if cpp.get_id() == 'clang' and cpp_is_microsoft_compiler
    add_project_arguments('-std=c++14', language: 'cpp')
  elif cpp.get_id() == 'clang-cl'
    # Clang-cl produces a warning when using -std=c++14, but not when using /std:c++14
    add_project_arguments('/std:c++14', language : 'cpp')
  endif
endif

if cpp.get_argument_syntax() == 'msvc'
  # Ignore several spurious warnings for things HarfBuzz does very commonly.
  # If a warning is completely useless and spammy, use '/wdXXXX' to suppress it
  # If a warning is harmless but hard to fix, use '/woXXXX' so it's shown once
  # NOTE: Only add warnings here if you are sure they're spurious
  msvc_args = [
    '/wd4244', # lossy type conversion (e.g. double -> int)
    '/bigobj', # hb-subset.cc -- compile error C1128: number of sections exceeded object file format limit
    cpp.get_supported_arguments(['/utf-8']), # set the input encoding to utf-8
  ]
  add_project_arguments(msvc_args, language: ['c', 'cpp'])
  # Disable SAFESEH with MSVC for libs that use external deps that are built with MinGW
  # noseh_link_args = ['/SAFESEH:NO']
endif

add_project_link_arguments(cpp.get_supported_link_arguments([
  '-Bsymbolic-functions'
]), language: 'c')

add_project_arguments(cpp.get_supported_arguments([
  '-fno-exceptions',
  '-fno-rtti',
  '-fno-threadsafe-statics',
  '-fvisibility-inlines-hidden',
]), language: 'cpp')

if host_machine.cpu_family() == 'arm' and cpp.alignment('struct { char c; }') != 1
  if cpp.has_argument('-mstructure-size-boundary=8')
    add_project_arguments('-mstructure-size-boundary=8', language: 'cpp')
  endif
endif

if host_machine.system() == 'windows'
  add_project_arguments(cpp.get_supported_arguments([
    '-Wa,-mbig-obj'
  ]), language : 'cpp')
endif

check_headers = [
  ['unistd.h'],
  ['sys/mman.h'],
  ['stdbool.h'],
  ['xlocale.h'],
]

check_funcs = [
  ['atexit', {'prefix': '#include <stdlib.h>'}],
  ['mprotect', {'prefix': '#include <sys/mman.h>'}],
  ['sysconf', {'prefix': '#include <unistd.h>'}],
  ['getpagesize', {'prefix': '#include <unistd.h>'}],
  ['mmap', {'prefix': '#include <sys/mman.h>'}],
  ['isatty', {'prefix': '#include <unistd.h>'}],
  ['uselocale', {'prefix': '#include <locale.h>'}],
  ['newlocale', {'prefix': '#include <locale.h>'}],
  ['sincos', {'prefix': '#define _GNU_SOURCE\n#include <math.h>'}],
  ['sincosf', {'prefix': '#define _GNU_SOURCE\n#include <math.h>'}],
]

m_dep = cpp.find_library('m', required: false)

# Painful hack to handle multiple dependencies but also respect options
if get_option('freetype').disabled()
  freetype_dep = dependency('', required: false)
else
  # Sadly, FreeType's versioning schemes are different between pkg-config and CMake

  # Try pkg-config name
  freetype_dep = dependency('freetype2',
                            version: freetype_min_version,
                            method: 'pkg-config',
                            required: false,
                            allow_fallback: false)
  if not freetype_dep.found()
    # Try cmake name
    freetype_dep = dependency('Freetype',
                              version: freetype_min_version_actual,
                              method: 'cmake',
                              required: false,
                              allow_fallback: false)
    # Subproject fallback
    if not freetype_dep.found()
      freetype_proj = subproject('freetype2',
                                version: freetype_min_version_actual,
                                required: get_option('freetype'),
                                default_options: ['harfbuzz=disabled'])
      if freetype_proj.found()
        freetype_dep = freetype_proj.get_variable('freetype_dep')
      else
        freetype_dep = dependency('', required: false)
      endif
    endif
  endif
endif

glib_dep = dependency('glib-2.0', version: glib_min_version, required: get_option('glib'))
gobject_dep = dependency('gobject-2.0', version: glib_min_version, required: get_option('gobject'))
graphite2_dep = dependency('graphite2', version: graphite2_min_version, required: get_option('graphite2'))
graphite_dep = dependency('graphite2', version: graphite2_min_version, required: get_option('graphite'))
wasm_dep = cpp.find_library('iwasm', required: get_option('wasm'))
# How to check whether iwasm was built, and hence requires, LLVM?
#llvm_dep = cpp.find_library('LLVM-15', required: get_option('wasm'))

# pkg-config: icu-uc, cmake: ICU but with components
icu_dep = dependency('icu-uc', 'ICU',
                          version: icu_min_version,
                          components: 'uc',
                          required: get_option('icu'),
                          allow_fallback: true)

if icu_dep.found() and icu_dep.version().version_compare('>=75.1') and (get_option('cpp_std') == 'c++11' or get_option('cpp_std') == 'c++14')
  cpp17_arg = cpp.get_argument_syntax() == 'msvc' ? '/std:c++17' : '-std=c++17'
  add_project_arguments(cpp17_arg, language: 'cpp')
endif

if icu_dep.found() and icu_dep.type_name() == 'pkgconfig'
  icu_defs = icu_dep.get_variable(pkgconfig: 'DEFS', default_value: '').split()
  if icu_defs.length() > 0
    add_project_arguments(icu_defs, language: ['c', 'cpp'])
  endif
endif

cairo_dep = null_dep
cairo_ft_dep = null_dep
if not get_option('cairo').disabled()
  cairo_dep = dependency('cairo', version: cairo_min_version, required: false)
  cairo_ft_dep = dependency('cairo-ft', version: cairo_min_version, required: false)

  if (not cairo_dep.found() and
      cpp.get_argument_syntax() == 'msvc' and
      cpp.has_header('cairo.h'))
    cairo_dep = cpp.find_library('cairo', required: false)
    if cairo_dep.found() and cpp.has_function('cairo_ft_font_face_create_for_ft_face',
                                              prefix: '#include <cairo-ft.h>',
                                              dependencies: cairo_dep)
      cairo_ft_dep = cairo_dep
    endif
  endif

  if not cairo_dep.found()
    # Note that we don't have harfbuzz -> cairo -> freetype2 -> harfbuzz fallback
    # dependency cycle here because we have configured freetype2 above with
    # harfbuzz support disabled, so when cairo will lookup freetype2 dependency
    # it will be forced to use that one.
    cairo_dep = dependency('cairo', version: cairo_min_version, required: get_option('cairo'))
    cairo_ft_required = get_option('cairo').enabled() and get_option('freetype').enabled()
    cairo_ft_dep = dependency('cairo-ft', version: cairo_min_version, required: cairo_ft_required)
  endif
endif

chafa_dep = dependency('chafa', version: chafa_min_version, required: get_option('chafa'))

fontations_dep_found = false
if get_option('fontations').enabled()
  add_languages(['rust'], native: false, required : true)
  fontations_dep_found = true
endif

harfrust_dep_found = false
if get_option('harfrust').enabled()
  add_languages(['rust'], native: false, required : true)
  harfrust_dep_found = true
endif

kbts_dep = null_dep
if get_option('kbts').enabled()
  kbts_dep = subproject('kbts').get_variable('kbts_dep')
endif

conf = configuration_data()
incconfig = include_directories('.')

add_project_arguments('-DHAVE_CONFIG_H', language: ['c', 'cpp'])

cpp_args = []

if glib_dep.found()
  conf.set('HAVE_GLIB', 1)
endif

if gobject_dep.found()
  conf.set('HAVE_GOBJECT', 1)
endif

if cairo_dep.found()
  conf.set('HAVE_CAIRO', 1)
  check_cairo_funcs = [
    ['cairo_user_font_face_set_render_color_glyph_func', {'deps': cairo_dep}],
    ['cairo_font_options_get_custom_palette_color', {'deps': cairo_dep}],
    ['cairo_user_scaled_font_get_foreground_source', {'deps': cairo_dep}],
  ]

  if cairo_dep.type_name() == 'internal'
    foreach func: check_cairo_funcs
      name = func[0]
      conf.set('HAVE_@0@'.format(name.to_upper()), 1)
    endforeach
  else
    check_funcs += check_cairo_funcs
  endif
endif

if cairo_ft_dep.found()
  conf.set('HAVE_CAIRO_FT', 1)
endif

if chafa_dep.found()
  conf.set('HAVE_CHAFA', 1)
endif

if fontations_dep_found
  conf.set('HAVE_FONTATIONS', 1)
endif

if harfrust_dep_found
  conf.set('HAVE_HARFRUST', 1)
endif

if kbts_dep.found()
  conf.set('HAVE_KBTS', 1)
endif

if wasm_dep.found()
  conf.set('HAVE_WASM', 1)
  conf.set('HB_WASM_MODULE_DIR', '"'+get_option('prefix')+'/'+get_option('libdir')+'/harfbuzz/wasm"')
endif

if graphite2_dep.found() or graphite_dep.found()
  conf.set('HAVE_GRAPHITE2', 1)
endif

if icu_dep.found()
  conf.set('HAVE_ICU', 1)
endif

if get_option('icu_builtin')
  conf.set('HAVE_ICU_BUILTIN', 1)
endif

if get_option('experimental_api')
  conf.set('HB_EXPERIMENTAL_API', 1)
endif

if freetype_dep.found()
  conf.set('HAVE_FREETYPE', 1)
  check_freetype_funcs = [
    ['FT_Get_Var_Blend_Coordinates', {'deps': freetype_dep}],
    ['FT_Set_Var_Blend_Coordinates', {'deps': freetype_dep}],
    ['FT_Done_MM_Var', {'deps': freetype_dep}],
    ['FT_Get_Transform', {'deps': freetype_dep}],
  ]

  if freetype_dep.type_name() == 'internal'
    foreach func: check_freetype_funcs
      name = func[0]
      conf.set('HAVE_@0@'.format(name.to_upper()), 1)
    endforeach
  else
    check_funcs += check_freetype_funcs
  endif
endif

gdi_uniscribe_deps = []
# GDI (Uniscribe) (Windows)
if host_machine.system() == 'windows' and not get_option('gdi').disabled()
  if (get_option('gdi').enabled() and
      not (cpp.has_header('usp10.h') and cpp.has_header('windows.h')))
    error('GDI/Uniscribe was enabled explicitly, but required headers are missing.')
  endif

  gdi_deps_found = true
  foreach usplib : ['usp10', 'gdi32', 'rpcrt4', 'user32']
    dep = cpp.find_library(usplib, required: get_option('gdi'))
    gdi_deps_found = gdi_deps_found and dep.found()
    gdi_uniscribe_deps += dep
  endforeach

  if gdi_deps_found
    conf.set('HAVE_UNISCRIBE', 1)
    conf.set('HAVE_GDI', 1)
  endif
endif

directwrite_dep = []
# DirectWrite (Windows)
if host_machine.system() == 'windows' and not get_option('directwrite').disabled()
  if get_option('directwrite').enabled() and not cpp.has_header('dwrite_3.h')
    error('DirectWrite was enabled explicitly, but required header is missing.')
  endif
  directwrite_dep = cpp.find_library('dwrite', required: get_option('directwrite'))
  if directwrite_dep.found()
    conf.set('HAVE_DIRECTWRITE', 1)
  endif
endif

# CoreText (macOS)
coretext_deps = []
if host_machine.system() == 'darwin' and not get_option('coretext').disabled()
  app_services_dep = dependency('appleframeworks', modules: ['ApplicationServices'], required: false)
  if cpp.has_type('CTFontRef', prefix: '#include <ApplicationServices/ApplicationServices.h>', dependencies: app_services_dep)
    coretext_deps += [app_services_dep]
    conf.set('HAVE_CORETEXT', 1)
  # On iOS CoreText and CoreGraphics are stand-alone frameworks
  # Check for a different symbol to avoid getting cached result
  else
    coretext_dep = dependency('appleframeworks', modules: ['CoreText'], required: false)
    coregraphics_dep = dependency('appleframeworks', modules: ['CoreGraphics'], required: false)
    corefoundation_dep = dependency('appleframeworks', modules: ['CoreFoundation'], required: false)
    if cpp.has_type('CTRunRef', prefix: '#include <CoreText/CoreText.h>', dependencies: [coretext_dep, coregraphics_dep, corefoundation_dep])
      coretext_deps += [coretext_dep, coregraphics_dep, corefoundation_dep]
      conf.set('HAVE_CORETEXT', 1)
    elif get_option('coretext').enabled()
      error('CoreText was enabled explicitly, but required headers or frameworks are missing.')
    endif
  endif
endif

# threads
thread_dep = null_dep
if host_machine.system() != 'windows'
  thread_dep = dependency('threads', required: false)

  if thread_dep.found()
    conf.set('HAVE_PTHREAD', 1)
  endif
endif

conf.set_quoted('PACKAGE_NAME', 'HarfBuzz')
conf.set_quoted('PACKAGE_VERSION', meson.project_version())

foreach check : check_headers
  name = check[0]

  if cpp.has_header(name)
    conf.set('HAVE_@0@'.format(name.to_upper().underscorify()), 1)
  endif
endforeach

harfbuzz_extra_deps = []
foreach check : check_funcs
  name = check[0]
  opts = check.get(1, {})
  link_withs = opts.get('link_with', [])
  check_deps = opts.get('deps', [])
  check_prefix = opts.get('prefix', '')
  extra_deps = []
  found = true

  # First try without linking
  found = cpp.has_function(name, prefix: check_prefix, dependencies: check_deps)

  if not found and link_withs.length() > 0
    found = true

    foreach link_with : link_withs
      dep = cpp.find_library(link_with, required: false)
      if dep.found()
        extra_deps += dep
      else
        found = false
      endif
    endforeach

    if found
      found = cpp.has_function(name, prefix: check_prefix, dependencies: check_deps + extra_deps)
    endif
  endif

  if found
    harfbuzz_extra_deps += extra_deps
    conf.set('HAVE_@0@'.format(name.to_upper()), 1)
  endif
endforeach

# CMake support (package install dir)

# Equivalent to configure_package_config_file(INSTALL_DESTINATION ...), see
# https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html#command:configure_package_config_file.
# In certain unusual packaging layouts such as Nixpkgs, the Harfbuzz package
# is installed into two Nix store paths, "out" and "dev", where "out" contains
# libraries only (i.e. lib/libharfbuzz.so) and "dev" contains development
# files, i.e. include and lib/cmake. If CMake package files are installed to
# "out", Nixpkgs will move them to "dev", which breaks assumptions about
# our file paths. Since we need to figure out relative install paths here
# to make a relocatable package, we do need to know the final path of our
# CMake files to calculate the correct relative paths.
# Of course, this still defaults to $libdir/cmake if unset, which works for
# most packaging layouts.
cmake_package_install_dir = get_option('cmakepackagedir')

if cmake_package_install_dir == ''
  cmake_package_install_dir = get_option('libdir') / 'cmake'
endif

subdir('src')

if not get_option('utilities').disabled()
  subdir('util')
endif

if not get_option('tests').disabled()
  subdir('test')
endif

if not get_option('benchmark').disabled()
  subdir('perf')
endif

if not get_option('docs').disabled()
  subdir('docs')
endif

configure_file(output: 'config.h', configuration: conf)

alias_target('lib', libharfbuzz)
alias_target('libs', libharfbuzz, libharfbuzz_subset)

# Re glib, see https://github.com/harfbuzz/harfbuzz/issues/4153#issuecomment-2646347531
add_test_setup('default',
                exclude_suites: ['google-benchmark'],
                is_default: glib_dep.type_name() != 'internal' and not meson.is_subproject())

build_summary = {
  'Directories':
    {'prefix': get_option('prefix'),
     'bindir': get_option('bindir'),
     'libdir': get_option('libdir'),
     'includedir': get_option('includedir'),
     'datadir': get_option('datadir'),
     'cmakepackagedir': cmake_package_install_dir
    },
  'Unicode callbacks (you want at least one)':
    {'Builtin': true,
     'Glib': conf.get('HAVE_GLIB', 0) == 1,
     'ICU': conf.get('HAVE_ICU', 0) == 1,
    },
  'Font callbacks (the more the merrier)':
    {'Builtin' : true,
     'FreeType': conf.get('HAVE_FREETYPE', 0) == 1,
     'Fontations': conf.get('HAVE_FONTATIONS', 0) == 1,
    },
  'Dependencies used for command-line utilities':
    {'Cairo': conf.get('HAVE_CAIRO', 0) == 1,
     'Chafa': conf.get('HAVE_CHAFA', 0) == 1,
    },
  'Additional shapers':
    {'Graphite2': conf.get('HAVE_GRAPHITE2', 0) == 1,
     'WebAssembly (experimental)': conf.get('HAVE_WASM', 0) == 1,
    },
  'Platform shapers (not normally needed)':
    {'CoreText': conf.get('HAVE_CORETEXT', 0) == 1,
     'DirectWrite': conf.get('HAVE_DIRECTWRITE', 0) == 1,
     'GDI/Uniscribe': (conf.get('HAVE_GDI', 0) == 1) and (conf.get('HAVE_UNISCRIBE', 0) == 1),
     'HarfRust': conf.get('HAVE_HARFRUST', 0) == 1,
     'kbts': conf.get('HAVE_KBTS', 0) == 1,
    },
  'Other features':
    {'Utilities': not get_option('utilities').disabled(),
     'Documentation': conf.get('HAVE_GTK_DOC', 0) == 1,
     'GObject bindings': conf.get('HAVE_GOBJECT', 0) == 1,
     'Cairo integration': conf.get('HAVE_CAIRO', 0) == 1,
     'Introspection': conf.get('HAVE_INTROSPECTION', 0) == 1,
     'Experimental APIs': conf.get('HB_EXPERIMENTAL_API', 0) == 1,
    },
  'Testing':
    {'Tests': get_option('tests').enabled(),
     'Benchmark': get_option('benchmark').enabled(),
    },
}
foreach section_title, section : build_summary
  summary(section, bool_yn: true, section: section_title)
endforeach
