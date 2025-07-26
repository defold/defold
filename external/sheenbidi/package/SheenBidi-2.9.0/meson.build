project('SheenBidi', ['c', 'cpp'],
  version: run_command(
    'Tools/Version/extract.py',
    'Headers/SheenBidi/SBVersion.h',
    check: true
  ).stdout().strip(),
  meson_version: '>= 0.55.0',
  default_options: [
    'cpp_std=c++14'
  ]
)

unity_mode = get_option('unity_mode').enabled()
build_generator = get_option('generator').enabled()

is_windows_host = host_machine.system() == 'windows'
static_mode = get_option('default_library') == 'static'
dll_mode = is_windows_host and not static_mode
build_tests = not dll_mode and not unity_mode

project_version = meson.project_version()
soversion = project_version.split('.')[0]

# ------------------------------------------------------------------------------
# SheenBidi Library
# ------------------------------------------------------------------------------

sheenbidi_headers = files(
  'Headers/SheenBidi/SBAlgorithm.h',
  'Headers/SheenBidi/SBBase.h',
  'Headers/SheenBidi/SBBidiType.h',
  'Headers/SheenBidi/SBCodepoint.h',
  'Headers/SheenBidi/SBCodepointSequence.h',
  'Headers/SheenBidi/SBGeneralCategory.h',
  'Headers/SheenBidi/SBLine.h',
  'Headers/SheenBidi/SBMirrorLocator.h',
  'Headers/SheenBidi/SBParagraph.h',
  'Headers/SheenBidi/SBRun.h',
  'Headers/SheenBidi/SBScript.h',
  'Headers/SheenBidi/SBScriptLocator.h',
  'Headers/SheenBidi/SBVersion.h',
  'Headers/SheenBidi/SheenBidi.h'
)
internal_headers = files(
  'Source/BidiChain.h',
  'Source/BidiTypeLookup.h',
  'Source/BracketQueue.h',
  'Source/BracketType.h',
  'Source/GeneralCategoryLookup.h',
  'Source/IsolatingRun.h',
  'Source/LevelRun.h',
  'Source/Object.h',
  'Source/PairingLookup.h',
  'Source/RunExtrema.h',
  'Source/RunKind.h',
  'Source/RunQueue.h',
  'Source/SBAlgorithm.h',
  'Source/SBAssert.h',
  'Source/SBBase.h',
  'Source/SBCodepoint.h',
  'Source/SBCodepointSequence.h',
  'Source/SBLine.h',
  'Source/SBLog.h',
  'Source/SBMirrorLocator.h',
  'Source/SBParagraph.h',
  'Source/SBScriptLocator.h',
  'Source/ScriptLookup.h',
  'Source/ScriptStack.h',
  'Source/StatusStack.h'
)

if unity_mode
  add_project_arguments('-DSB_CONFIG_UNITY', language: ['c', 'cpp'])
  sheenbidi_sources = files('Source/SheenBidi.c')
else
  sheenbidi_sources = files(
    'Source/BidiChain.c',
    'Source/BidiTypeLookup.c',
    'Source/BracketQueue.c',
    'Source/GeneralCategoryLookup.c',
    'Source/IsolatingRun.c',
    'Source/LevelRun.c',
    'Source/Object.c',
    'Source/PairingLookup.c',
    'Source/RunQueue.c',
    'Source/SBAlgorithm.c',
    'Source/SBBase.c',
    'Source/SBCodepoint.c',
    'Source/SBCodepointSequence.c',
    'Source/SBLine.c',
    'Source/SBLog.c',
    'Source/SBMirrorLocator.c',
    'Source/SBParagraph.c',
    'Source/SBScriptLocator.c',
    'Source/ScriptLookup.c',
    'Source/ScriptStack.c',
    'Source/StatusStack.c'
  )
endif

sheenbidi = library(
  'SheenBidi',
  sources: [sheenbidi_headers, internal_headers, sheenbidi_sources],
  c_args: dll_mode ? ['-DSB_CONFIG_DLL_EXPORT'] : [],
  include_directories: ['Headers', 'Source'],
  version: project_version,
  soversion: soversion,
  install: true
)

sheenbidi_dep = declare_dependency(
  include_directories: 'Headers',
  link_with: sheenbidi,
  compile_args: dll_mode ? ['-DSB_CONFIG_DLL_IMPORT'] : []
)

# ------------------------------------------------------------------------------
# Parser Library (Required for Generator/Tests)
# ------------------------------------------------------------------------------

if build_generator or build_tests
  parser_files = files(
    'Tools/Parser/BidiBrackets.cpp',
    'Tools/Parser/BidiBrackets.h',
    'Tools/Parser/BidiCharacterTest.cpp',
    'Tools/Parser/BidiCharacterTest.h',
    'Tools/Parser/BidiMirroring.cpp',
    'Tools/Parser/BidiMirroring.h',
    'Tools/Parser/BidiTest.cpp',
    'Tools/Parser/BidiTest.h',
    'Tools/Parser/DataFile.cpp',
    'Tools/Parser/DataFile.h',
    'Tools/Parser/DerivedBidiClass.cpp',
    'Tools/Parser/DerivedBidiClass.h',
    'Tools/Parser/DerivedCoreProperties.cpp',
    'Tools/Parser/DerivedCoreProperties.h',
    'Tools/Parser/DerivedGeneralCategory.cpp',
    'Tools/Parser/DerivedGeneralCategory.h',
    'Tools/Parser/PropertyValueAliases.cpp',
    'Tools/Parser/PropertyValueAliases.h',
    'Tools/Parser/PropList.cpp',
    'Tools/Parser/PropList.h',
    'Tools/Parser/Scripts.cpp',
    'Tools/Parser/Scripts.h',
    'Tools/Parser/UnicodeData.cpp',
    'Tools/Parser/UnicodeData.h',
    'Tools/Parser/UnicodeVersion.cpp',
    'Tools/Parser/UnicodeVersion.h'
  )
  
  parser = static_library(
    'Parser',
    sources: parser_files,
    include_directories: ['Tools']
  )
  
  parser_dep = declare_dependency(
    include_directories: 'Tools',
    link_with: parser
  )
endif

# ------------------------------------------------------------------------------
# Generator Tool (Optional)
# ------------------------------------------------------------------------------

if build_generator
  generator_files = files(
    'Tools/Generator/Utilities/ArrayBuilder.cpp',
    'Tools/Generator/Utilities/ArrayBuilder.h',
    'Tools/Generator/Utilities/Converter.cpp',
    'Tools/Generator/Utilities/Converter.h',
    'Tools/Generator/Utilities/FileBuilder.cpp',
    'Tools/Generator/Utilities/FileBuilder.h',
    'Tools/Generator/Utilities/Math.cpp',
    'Tools/Generator/Utilities/Math.h',
    'Tools/Generator/Utilities/StreamBuilder.cpp',
    'Tools/Generator/Utilities/StreamBuilder.h',
    'Tools/Generator/Utilities/TextBuilder.cpp',
    'Tools/Generator/Utilities/TextBuilder.h',
    'Tools/Generator/BidiTypeLookupGenerator.cpp',
    'Tools/Generator/BidiTypeLookupGenerator.h',
    'Tools/Generator/GeneralCategoryLookupGenerator.cpp',
    'Tools/Generator/GeneralCategoryLookupGenerator.h',
    'Tools/Generator/main.cpp',
    'Tools/Generator/PairingLookupGenerator.cpp',
    'Tools/Generator/PairingLookupGenerator.h',
    'Tools/Generator/ScriptLookupGenerator.cpp',
    'Tools/Generator/ScriptLookupGenerator.h'
  )
  
  executable(
    'Generator',
    sources: generator_files,
    include_directories: ['Headers', 'Tools'],
    dependencies: [parser_dep],
    install: false
  )
endif

# ------------------------------------------------------------------------------
# Testing (Only non-unity mode)
# ------------------------------------------------------------------------------

if build_tests
  test_common_files = files(
    'Tests/Utilities/Convert.cpp',
    'Tests/Utilities/Convert.h',
    'Tests/Utilities/Unicode.h'
  )

  test_targets = {
    'AlgorithmTests': [
      'Tests/AlgorithmTests.h',
      'Tests/AlgorithmTests.cpp'
    ],
    'BidiTypeLookupTests': [
      'Tests/BidiTypeLookupTests.h',
      'Tests/BidiTypeLookupTests.cpp'
    ],
    'BracketLookupTests': [
      'Tests/BracketLookupTests.h',
      'Tests/BracketLookupTests.cpp'
    ],
    'CodepointSequenceTests': [
      'Tests/CodepointSequenceTests.h',
      'Tests/CodepointSequenceTests.cpp'
    ],
    'CodepintTests': [
      'Tests/CodepointTests.h',
      'Tests/CodepointTests.cpp'
    ],
    'GeneralCategoryLookupTests': [
      'Tests/GeneralCategoryLookupTests.h',
      'Tests/GeneralCategoryLookupTests.cpp'
    ],
    'MirrorLookupTests': [
      'Tests/MirrorLookupTests.h',
      'Tests/MirrorLookupTests.cpp'
    ],
    'RunQueueTests': [
      'Tests/RunQueueTests.h',
      'Tests/RunQueueTests.cpp'
    ],
    'ScriptLocatorTests': [
      'Tests/ScriptLocatorTests.h',
      'Tests/ScriptLocatorTests.cpp'
    ],
    'ScriptLookupTests': [
      'Tests/ScriptLookupTests.h',
      'Tests/ScriptLookupTests.cpp'
    ],
    'ScriptTests': [
      'Tests/ScriptTests.h',
      'Tests/ScriptTests.cpp'
    ]
  }

  foreach test_name, test_files : test_targets
    test_exe = executable(
      test_name,
      sources: test_common_files + test_files,
      include_directories: ['Headers', 'Tools'],
      dependencies: [parser_dep, sheenbidi_dep],
      cpp_args: ['-DSTANDALONE_TESTING'],
      install: false
    )
    test(
      test_name,
      test_exe,
      args: [meson.current_source_dir() / 'Tools/Unicode'],
      timeout: 60
    )
  endforeach
endif

# ------------------------------------------------------------------------------
# Installation
# ------------------------------------------------------------------------------

install_headers(
  sheenbidi_headers,
  subdir: 'SheenBidi'
)

pkg = import('pkgconfig')
pkg.generate(
  filebase: 'sheenbidi',
  name: 'SheenBidi',
  description: 'SheenBidi is a lightweight, fast and stable implementation of the Unicode Bidirectional Algorithm.',
  url: 'https://github.com/Tehreer/SheenBidi',
  version: project_version,
  libraries: sheenbidi,
  extra_cflags: dll_mode ? ['-DSB_CONFIG_DLL_IMPORT'] : []
)
