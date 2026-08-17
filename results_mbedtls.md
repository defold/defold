
MbedTls v2.17

iOS (mbedtls)

    3518472 engine/engine/build/src/dmengine

Android (mbedtls)

    4704056 engine/engine/build/src/libdmengine.so

Wasm

     280312 engine/engine/build/src/dmengine.js
     343412 engine/engine/build/src/dmengine.js.symbols
    2914175 engine/engine/build/src/dmengine.wasm



MbedTls v3.6.4

iOS (darwin crypt/sslsocket backend)

    3339152 engine/engine/build/src/dmengine (stripped)

Android (mbedtls crypt/sslsocket backend)

    4907528 engine/engine/build/src/libdmengine.so (stripped)

Wasm (mbedtls crypt backend)

     280312 engine/engine/build/src/dmengine.js
     343412 engine/engine/build/src/dmengine.js.symbols
    2914454 engine/engine/build/src/dmengine.wasm


MbedTls v4.1.0

iOS (darwin crypt/sslsocket backend)

    3340032 engine/engine/build/src/dmengine (stripped)

Android

    4859792 engine/engine/build/src/libdmengine.so (stripped)


Wasm (mbedtls crypt backend)

     280312 engine/engine/build/src/dmengine.js
     343502 engine/engine/build/src/dmengine.js.symbols
    2924310 engine/engine/build/src/dmengine.wasm


full android:

5054872 - 4704056 = 350816 +342kb


ios with mbedtls:

3623608 - 3340032 = 283576 +277kb

*********************************************************************

diff v2.17 -> v3.6.4

* iOS:      3339152 - 3518472 = -179320  -175kb
* Android:  4907528 - 4704056 = 203472  +199kb
* Wasm:     2914454 - 2914175 = 279


diff v2.17 -> v4.1.0

* iOS:      3340032 - 3518472 = -178440 -174kb
* Android:  4859792 - 4704056 = 155736 +152kb
* Wasm:     2924310 - 2914175 = 10135 +9.9kb


| Platform | v2.17 | v4.1.0 | Diff |
|---|---:|---:|---:|
| iOS | 3,518,472 | 3,340,032 | -174 kb |
| Android | 4,704,056 | 4,859,792 | +152 kb |
| Wasm | 2,914,175 | 2,924,310 | +9.9 kb |

# Create a separate PR for dead-stripping !









