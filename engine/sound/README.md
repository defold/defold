# Sound

The mixer sends audio to the device backend selected in the [sound CMake project](CMakeLists.txt). The default backends for the public platforms are:

| Platform | Output backend |
| --- | --- |
| macOS and iOS | [AVAudioEngine](src/devices/device_avaudio.mm) |
| Windows | [WASAPI](src/devices/device_wasapi.cpp) |
| Linux | [OpenAL](src/devices/device_openal.cpp) |
| Android | [OpenSL ES](src/devices/device_opensl.cpp) |
| HTML5 | [Web Audio](src/js/library_sound.js) |

The [engine CMake project](../engine/CMakeLists.txt) selects the sound device and decoders for each engine variant. The headless engine uses the null sound implementation.
