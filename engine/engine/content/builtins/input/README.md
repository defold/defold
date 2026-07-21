# SDL Game Controller Database

This file contains game controller mappings from the [SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB) project.

## Origin

- **Source**: https://github.com/mdqinc/SDL_GameControllerDB
- **Format**: SDL 2.0.16 gamecontrollerdb.txt format
- **Purpose**: Provides cross-platform game controller mappings for GLFW/SDL

## License

Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from the
use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would
   be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and must not
   be misrepresented as being the original software.

3. This notice may not be removed or altered from any source distribution.

## Usage

Defold automatically loads this database at runtime and filters mappings for
the target platform (Windows, macOS, Linux, iOS, Android). Platform-specific
mappings are used to provide consistent game controller input across platforms.

The file format is:
```
GUID,Name,mappings,platform:PlatformName,
```

Where:
- `GUID` is the 32-character SDL controller GUID
- `Name` is the controller name
- `mappings` are SDL-style button/axis/hat mappings
- `platform` filters mappings for specific platforms
