# SheenBidi

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Linux CI](https://github.com/Tehreer/SheenBidi/actions/workflows/linux.yml/badge.svg)](https://github.com/Tehreer/SheenBidi/actions/workflows/linux.yml)
[![macOS CI](https://github.com/Tehreer/SheenBidi/actions/workflows/macos.yml/badge.svg)](https://github.com/Tehreer/SheenBidi/actions/workflows/macos.yml)
[![Windows CI](https://github.com/Tehreer/SheenBidi/actions/workflows/windows.yml/badge.svg)](https://github.com/Tehreer/SheenBidi/actions/workflows/windows.yml)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/k2vvegcdqsb9ld5a?svg=true)](https://ci.appveyor.com/project/mta452/sheenbidi)
[![Coverage Status](https://coveralls.io/repos/github/Tehreer/SheenBidi/badge.svg?branch=master)](https://coveralls.io/github/Tehreer/SheenBidi)

SheenBidi is a lightweight, fast and stable implementation of the [Unicode Bidirectional Algorithm (UBA)](https://unicode.org/reports/tr9/).

It is being used by multiple open source and commercial projects in different domains such as:
- **Games:** [SuperTuxKart](https://github.com/supertuxkart/stk-code), [VVVVVV](https://github.com/TerryCavanagh/VVVVVV), [DevilutionX](https://github.com/diasurgical/DevilutionX), [Watch Dogs: Legion](https://www.mobygames.com/game/152206/watch-dogs-legion/), [Monaco 2](https://www.mobygames.com/game/240348/monaco-2/)
- **Audio:** [JUCE](https://github.com/juce-framework/JUCE), [Tracktion Engine](https://github.com/Tracktion/tracktion_engine), [d&b En-Space](https://www.dbaudio.com/global/en/solutions/enabling-technologies/sound-design/en-space/)
- **Mapping:** [VTS Browser](https://github.com/melowntech/vts-browser-cpp)
- **Animation:** [Rive Runtime](https://github.com/rive-app/rive-runtime)
- **Text Processing:** [Raqm](https://github.com/HOST-Oman/libraqm), [Rich Text](https://github.com/forenoonwatch/rich-text), [Tehreer Android](https://github.com/Tehreer/Tehreer-Android), [Tehreer Cocoa](https://github.com/Tehreer/Tehreer-Cocoa)
- **Web:** [Dropflow](https://github.com/chearon/dropflow), [Itemizer](https://github.com/chearon/itemizer)

*If you use SheenBidi in your project, I’d love to hear about it!*

## Features
- **Object Based Design:** Facilitates modular and maintainable code.
- **Core Level Optimization:** Ensures high performance.
- **Thread Safe Architecture:** Suitable for multithreaded applications.
- **Lightweight API:** Simplifies integration.
- **Encoding Support:** UTF-8, UTF-16, and UTF-32.

## API Overview
<img src="https://user-images.githubusercontent.com/2664112/39663208-716af1c4-5088-11e8-855c-ababe3e58c58.png" width="350">

The above diagram provides a visual representation of the API applied to sample text.

### SBCodepointSequence
Decodes a string buffer in a specified encoding into code points.

### SBAlgorithm
Determines the bidirectional type of each code unit in the source string. It identifies paragraph boundaries as per rule [P1](https://www.unicode.org/reports/tr9/#P1) and allows creation of paragraph objects with explicit base levels or derived from rules [P2](https://www.unicode.org/reports/tr9/#P2)-[P3](https://www.unicode.org/reports/tr9/#P3).

### SBParagraph
Represents a single paragraph processed with rules [X1](https://www.unicode.org/reports/tr9/#X1)-[I2](https://www.unicode.org/reports/tr9/#I2), providing resolved embedding levels for all code units.

### SBLine
Represents a line of text processed with rules [L1](https://www.unicode.org/reports/tr9/#L1)-[L2](https://www.unicode.org/reports/tr9/#L2), offering reordered level runs instead of individual characters.

### SBRun
Denotes a sequence of characters sharing the same embedding level. A run's direction is right-to-left if its embedding level is odd.

### SBMirrorLocator
Identifies mirrored characters in a line as determined by rule [L4](https://www.unicode.org/reports/tr9/#L4).

### SBScriptLocator
Assists in text shaping by locating script runs as specified in [UAX #24](https://www.unicode.org/reports/tr24/).

## Dependency
SheenBidi relies solely on standard C library headers: `stddef.h`, `stdint.h`, and `stdlib.h`.

## Configuration
Configuration options are available in `Headers/SBConfig.h`:

- `SB_CONFIG_LOG`: Logs activities performed during the bidirectional algorithm application.
- `SB_CONFIG_UNITY`: Builds the library as a single module, allowing the compiler to inline functions.

## Compiling
SheenBidi can be compiled with any C compiler. The recommended approach is to add all files to an IDE and build. If `SB_CONFIG_UNITY` is enabled, only `Source/SheenBidi.c` should be compiled.

### CMake
To build and install SheenBidi using CMake:

```bash
cmake -S. -Bbuild-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
sudo cmake --install build-release
```

For development and testing:

```bash
cmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Debug -DSB_CONFIG_UNITY=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

In other CMake projects, SheenBidi can be found via `find_package(SheenBidi)`, providing the target `SheenBidi::SheenBidi`. SheenBidi can also be used via `FetchContent`.

### Meson
To build and install SheenBidi using Meson:

```bash
meson setup builddir --buildtype=release
meson compile -C builddir
sudo meson install -C builddir
```

For testing:

```bash
meson setup builddir -Dunity_mode=disabled
meson test -C builddir --print-errorlogs
```

## Example
A simple example in C11.

```c
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SheenBidi/SheenBidi.h>

int main(int argc, const char * argv[]) {
    /* Create code point sequence for a sample bidirectional text. */
    const char *bidiText = "یہ ایک )car( ہے۔";
    SBCodepointSequence codepointSequence = { SBStringEncodingUTF8, (void *)bidiText, strlen(bidiText) };

    /* Extract the first bidirectional paragraph. */
    SBAlgorithmRef bidiAlgorithm = SBAlgorithmCreate(&codepointSequence);
    SBParagraphRef firstParagraph = SBAlgorithmCreateParagraph(bidiAlgorithm, 0, INT32_MAX, SBLevelDefaultLTR);
    SBUInteger paragraphLength = SBParagraphGetLength(firstParagraph);

    /* Create a line consisting of the whole paragraph and get its runs. */
    SBLineRef paragraphLine = SBParagraphCreateLine(firstParagraph, 0, paragraphLength);
    SBUInteger runCount = SBLineGetRunCount(paragraphLine);
    const SBRun *runArray = SBLineGetRunsPtr(paragraphLine);

    /* Log the details of each run in the line. */
    for (SBUInteger i = 0; i < runCount; i++) {
        printf("Run Offset: %ld\n", (long)runArray[i].offset);
        printf("Run Length: %ld\n", (long)runArray[i].length);
        printf("Run Level: %ld\n\n", (long)runArray[i].level);
    }

    /* Create a mirror locator and load the line in it. */
    SBMirrorLocatorRef mirrorLocator = SBMirrorLocatorCreate();
    SBMirrorLocatorLoadLine(mirrorLocator, paragraphLine, (void *)bidiText);
    const SBMirrorAgent *mirrorAgent = SBMirrorLocatorGetAgent(mirrorLocator);

    /* Log the details of each mirror in the line. */
    while (SBMirrorLocatorMoveNext(mirrorLocator)) {
        printf("Mirror Index: %ld\n", (long)mirrorAgent->index);
        printf("Actual Code Point: %ld\n", (long)mirrorAgent->codepoint);
        printf("Mirrored Code Point: %ld\n\n", (long)mirrorAgent->mirror);
    }

    /* Release all objects. */
    SBMirrorLocatorRelease(mirrorLocator);
    SBLineRelease(paragraphLine);
    SBParagraphRelease(firstParagraph);
    SBAlgorithmRelease(bidiAlgorithm);

    return 0;
}
```

## Support
If SheenBidi plays a role in your project, you’re welcome to [star the repository](https://github.com/Tehreer/SheenBidi/stargazers) or [contribute improvements](https://github.com/Tehreer/SheenBidi/pulls). Engagement from the community helps inform future development and ensures continued relevance.

## License
```
Copyright (C) 2014-2025 Muhammad Tayyab Akram

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
