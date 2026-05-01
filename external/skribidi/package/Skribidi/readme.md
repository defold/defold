![screenshot of some text edited with the sample program](/screenshots/skribidi.gif?raw=true)

# Skribidi
Skribidi is nimble bidirectional text stack for building UIs.

## Features
- bidirectional text layout
- bidirectional text editing
- font collections with CSS inspired font selection
- color emojis
- line breaking
- text attributes
    - size, weight, style, stretch, letter spacing, word spacing, line spacing, baseline align, horizontal align)
- icons
    - [PicoSVG](https://github.com/googlefonts/picosvg) or procedural
- glyph, emoji and icon rasterization
    - color, SDF and alpha
- render cache with image atlas for glyphs and icons
- layout cache for immediate mode use
- lean dependencies

## Motivation
Text rendering is hard, text editing is hard too, font selection is hard, text alignment is hard.
Everything text related is hard, and messy, and full of nuances.

If you wanted to have bidirection text display and input for you project, there are not many options.
There are a lot of great projects that cover parts of the text stack, but there's still huge amount
of work left piece things together. A lot of the wisdom is scattered all over old browser bugs,
blogs that might not exists anymore, or bits of code scattered all over the world.

Many text stacks are part of larger projects, like browsers or game engines,
or have licenses that are not permissive, or require huge dependecies.

Skribidi tries to solve the text stack for UIs without dragging in large dependencies.
Text layout, text input, and font rasterization, with features that you'd expect to build an UI.

Skribidi leans heavity on [Harfbuzz](https://github.com/harfbuzz/harfbuzz) for text shaping and accessing font data,
[SheenBidi](https://github.com/Tehreer/SheenBidi) for bidirectional segmentation,
[libunibreak](https://github.com/adah1972/libunibreak) for grapheme and linebreak detection,
and [budouxc](https://github.com/memononen/budouxc) for East Asian word boundary detection.
All permissively licensed and quite lean dependencies.

## Status
Skribidi just got started. There are bugs and the API is very likely to change.

## Building
- Install [CMake](https://cmake.org/)
- Ensure CMake is in the user `PATH`
- `mkdir build`
- `cd build`
- `cmake ..`
- Build
	- *Windows*: Open and build `build/skribidi.sln`
	- *Linux*: use `cmake --build . -j$(nproc)`
	- *macOS*: use `cmake --build . -j$(sysctl -n hw.ncpu)`

When running the example or test, the working directory should be the build binary directory (`/build/bin`). On Windows, the example data direction is copied there and on Linux or macOS there's a symlink for the data directory.

## Dependencies
The project uses CMake, but you dont need to. If you handle dependecies yourself you can just add the
`include` and `src` to your project and you're good to go. The CMake is used to fetch the right deps
and to build the examples and tests, making development simpler.

- [Harfbuzz](https://github.com/harfbuzz/harfbuzz) - 11.0.0
- [SheenBidi](https://github.com/Tehreer/SheenBidi) - 83f77108a2873600283f6da4b326a2dca7a3a7a6
- [libunibreak](https://github.com/adah1972/libunibreak) - libunibreak_6_1.zip
- [budouxc](https://github.com/memononen/budouxc) - a044d49afc654117fac7623fff15bec15943270c

## License
Skribidi is developed by Mikko Mononen and uses the [MIT license](https://en.wikipedia.org/wiki/MIT_License).

## Similar or Related Projects
- [Pango](https://www.gtk.org/docs/architecture/pango) (C)
- [Cosmic Text](https://github.com/pop-os/cosmic-text) (Rust)
- [Harfbuzz](https://github.com/harfbuzz/harfbuzz)
- [SheenBidi](https://github.com/Tehreer/SheenBidi)
- [libunibreak](https://github.com/adah1972/libunibreak)
- [budouxc](https://github.com/memononen/budouxc)

# Example fonts
The examples use following fonts:
- [IBM Plex Sans](https://fonts.google.com/specimen/IBM+Plex+Sans) (Latin, Arabic, Devanagari, Hebrew, Japanese, Korean)
- [Noto](https://fonts.google.com/noto) (Bengali, Brahmi, Tamil, Thai, Balinese)
- [Noto Color Emoji](https://fonts.google.com/noto/specimen/Noto+Color+Emoji)
- [OpenMoji](https://openmoji.org/)
