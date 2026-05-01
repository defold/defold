Sound
=====

OpenAL
------

Different OpenAL implementations currently are used for the supported platform:

* iOS - native
* Mac OS X - native
* Android - OpenAL Soft (built from source)
* Linux - native

OpenAL soft
-----------

A configuration file the target platform is required in *src/openal/config/PLATFORM/config.h*

Wasapi
------

On windows we use Wasapi sound api


Android latency issues
----------------------

We currently have latency issues on Android. One way to reduce the latency is to 
set 

	device->NumUpdates = 1;
    device->UpdateSize = 64;

in Alc.c. The impact on low-end devices is however unknown and the change is only tested
on Samsung Galaxy S2.

**Links:**

* [http://www.youtube.com/watch?v=d3kfEeMZ65c]()
* [https://code.google.com/p/high-performance-audio]()
* [http://stackoverflow.com/questions/14842803/low-latency-audio-playback-on-android]()

Future
------

* Change to OpenAL Soft on all platforms
* Support Neon on Android. Detection of Neon instruction set is probably required
