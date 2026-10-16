Sound
=====

HTML5 audio activation
----------------------

The Web Audio device waits for user activation before resuming a suspended
context on a top-level page. Keyboard and pointer handlers retry inside the
browser event handler. Once a context has run, the device also retries after
interruptions and output-device changes.

Embedded games must additionally support [delegated autoplay](https://developer.chrome.com/blog/autoplay/#iframe-delegation).
A parent can gain and delegate permission after the child's context was created,
without setting the child's `navigator.userActivation.hasBeenActive`. Background
retries therefore allow the browser to decide whether an iframe may resume.
This can produce an autoplay warning in a fresh, blocked iframe. It is a
compatibility tradeoff: removing that attempt would prevent delegated playback
until the user interacts with the game itself. A pending resume is not retried
by the periodic timer or by non-activating input such as touchstart or Escape;
an activating gesture or a device-change event can retry it.

Check these cases in a fresh browser context: top-level startup, keyboard-only
activation, blocked iframe startup, and cross-origin autoplay delegated after
context creation. The JavaScript unit tests cover state transitions and retry
counts, but their fake AudioContext cannot validate browser permission policy
or browser-generated console warnings.

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
