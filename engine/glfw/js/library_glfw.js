/*******************************************************************************
 * EMSCRIPTEN GLFW 2.7.7 emulation.
 * It tries to emulate the behavior described in
 * http://www.glfw.org/GLFWReference277.pdf
 *
 * What it does:
 * - Creates a GL context.
 * - Manage keyboard and mouse events.
 * - GL Extensions support.
 *
 * What it does not but should probably do:
 * - Transmit events when glfwPollEvents, glfwWaitEvents or glfwSwapBuffers is
 *    called. Events callbacks are called as soon as event are received.
 * - Thread emulation.
 * - Image/Texture I/O support (that is deleted in GLFW 3).
 * - Video modes detection.
 *
 * Authors:
 * - Éloi Rivard <eloi.rivard@gmail.com>
 * - Thomas Borsos <thomasborsos@gmail.com>
 *
 ******************************************************************************/

var LibraryGLFW = {
  $GLFW: {

    keyFunc: null,
    charFunc: null,
    markedTextFunc: null,
    gamepadFunc:null,
    mouseButtonFunc: null,
    mousePosFunc: null,
    mouseWheelFunc: null,
    resizeFunc: null,
    closeFunc: null,
    refreshFunc: null,
    focusFunc: null,
    iconifyFunc: null,
    touchFunc: null,
    params: null,
    initTime: null,
    wheelPos: 0,
    buttons: 0,
    keys: 0,
    initWindowWidth: 640,
    initWindowHeight: 480,
    windowX: 0,
    windowY: 0,
    windowWidth: 0,
    windowHeight: 0,
    prevWidth: 0,
    prevHeight: 0,
    prevNonFSWidth: 0,
    prevNonFSHeight: 0,
    isFullscreen: false,
    isPointerLocked: false,
    dpi: 1,
    mouseTouchId:null,

/*******************************************************************************
 * DOM EVENT CALLBACKS
 ******************************************************************************/

    DOMToGLFWKeyCode: function(keycode, code) {
      switch (keycode) {
        case 0x08: return 295 ; // DOM_VK_BACKSPACE -> GLFW_KEY_BACKSPACE
        case 0x09: return 293 ; // DOM_VK_TAB -> GLFW_KEY_TAB
        case 0x0D: return 294 ; // DOM_VK_ENTER -> GLFW_KEY_ENTER
        case 0x1B: return 257 ; // DOM_VK_ESCAPE -> GLFW_KEY_ESC
        case 0x6A: return 313 ; // DOM_VK_MULTIPLY -> GLFW_KEY_KP_MULTIPLY
        case 0x6B: return 315 ; // DOM_VK_ADD -> GLFW_KEY_KP_ADD
        case 0x6D: return 314 ; // DOM_VK_SUBTRACT -> GLFW_KEY_KP_SUBTRACT
        case 0x6E: return 316 ; // DOM_VK_DECIMAL -> GLFW_KEY_KP_DECIMAL
        case 0x6F: return 312 ; // DOM_VK_DIVIDE -> GLFW_KEY_KP_DIVIDE
        case 0x70: return 258 ; // DOM_VK_F1 -> GLFW_KEY_F1
        case 0x71: return 259 ; // DOM_VK_F2 -> GLFW_KEY_F2
        case 0x72: return 260 ; // DOM_VK_F3 -> GLFW_KEY_F3
        case 0x73: return 261 ; // DOM_VK_F4 -> GLFW_KEY_F4
        case 0x74: return 262 ; // DOM_VK_F5 -> GLFW_KEY_F5
        case 0x75: return 263 ; // DOM_VK_F6 -> GLFW_KEY_F6
        case 0x76: return 264 ; // DOM_VK_F7 -> GLFW_KEY_F7
        case 0x77: return 265 ; // DOM_VK_F8 -> GLFW_KEY_F8
        case 0x78: return 266 ; // DOM_VK_F9 -> GLFW_KEY_F9
        case 0x79: return 267 ; // DOM_VK_F10 -> GLFW_KEY_F10
        case 0x7a: return 268 ; // DOM_VK_F11 -> GLFW_KEY_F11
        case 0x7b: return 269 ; // DOM_VK_F12 -> GLFW_KEY_F12
        case 0x25: return 285 ; // DOM_VK_LEFT -> GLFW_KEY_LEFT
        case 0x26: return 283 ; // DOM_VK_UP -> GLFW_KEY_UP
        case 0x27: return 286 ; // DOM_VK_RIGHT -> GLFW_KEY_RIGHT
        case 0x28: return 284 ; // DOM_VK_DOWN -> GLFW_KEY_DOWN
        case 0x21: return 298 ; // DOM_VK_PAGE_UP -> GLFW_KEY_PAGEUP
        case 0x22: return 299 ; // DOM_VK_PAGE_DOWN -> GLFW_KEY_PAGEDOWN
        case 0x24: return 300 ; // DOM_VK_HOME -> GLFW_KEY_HOME
        case 0x23: return 301 ; // DOM_VK_END -> GLFW_KEY_END
        case 0x2d: return 296 ; // DOM_VK_INSERT -> GLFW_KEY_INSERT
        case 16  : return 287 ; // DOM_VK_SHIFT -> GLFW_KEY_LSHIFT
        case 0x05: return 287 ; // DOM_VK_LEFT_SHIFT -> GLFW_KEY_LSHIFT
        case 0x06: return 288 ; // DOM_VK_RIGHT_SHIFT -> GLFW_KEY_RSHIFT
        case 17  : return 289 ; // DOM_VK_CONTROL -> GLFW_KEY_LCTRL
        case 0x03: return 289 ; // DOM_VK_LEFT_CONTROL -> GLFW_KEY_LCTRL
        case 0x04: return 290 ; // DOM_VK_RIGHT_CONTROL -> GLFW_KEY_RCTRL
        case 18  : return 291 ; // DOM_VK_ALT -> GLFW_KEY_LALT
        case 0x02: return 291 ; // DOM_VK_LEFT_ALT -> GLFW_KEY_LALT
        case 0x01: return 292 ; // DOM_VK_RIGHT_ALT -> GLFW_KEY_RALT
        case 96  : return 302 ; // GLFW_KEY_KP_0
        case 97  : return 303 ; // GLFW_KEY_KP_1
        case 98  : return 304 ; // GLFW_KEY_KP_2
        case 99  : return 305 ; // GLFW_KEY_KP_3
        case 100 : return 306 ; // GLFW_KEY_KP_4
        case 101 : return 307 ; // GLFW_KEY_KP_5
        case 102 : return 308 ; // GLFW_KEY_KP_6
        case 103 : return 309 ; // GLFW_KEY_KP_7
        case 104 : return 310 ; // GLFW_KEY_KP_8
        case 105 : return 311 ; // GLFW_KEY_KP_9
      }

      // Map additional keys not already mapped to any GLFW keys
      // We use KeyEvent.code here as it represents a physical key on the keyboard
      switch (code) {
        case "Minus":         return 45  ; // -
        case "Period":        return 46  ; // .
        case "Comma":         return 44  ; // ,
        case "Slash":         return 47  ; // /
        case "Backslash":     return 92  ; // \
        case "IntlRo":        return 92  ; // \ https://www.w3.org/TR/uievents-code/#keyboard-104
        case "IntlYen":       return 92  ; // \ https://www.w3.org/TR/uievents-code/#keyboard-101alt
        case "IntlBackslash": return 92  ; // \ https://www.w3.org/TR/uievents-code/#keyboard-102
        case "Backquote":     return 96  ; // `
        case "BracketLeft":   return 91  ; // [
        case "BracketRight":  return 93  ; // ]
        case "Equal":         return 61  ; // =
        case "Quote":         return 39  ; // '
        case "Semicolon":     return 59  ; // ;
        case "NumpadComma":   return 316 ; // GLFW_KEY_KP_DECIMAL, https://www.w3.org/TR/uievents-code/#keyboard-104
      }

      return keycode;
    },

    // The button ids for right and middle are swapped between GLFW and JS.
    // JS: right = 2, middle = 1
    // GLFW: right = 1, middle = 2
    // Use this function to convert between JS and GLFW, and back.
    DOMtoGLFWButton: function(button) {
      if (button == 1) {
        button = 2;
      } else if (button == 2) {
        button = 1;
      }
      return button;
    },

    // UCS-2 to UTF16 (ISO 10646)
    getUnicodeChar: function(value) {
      var output = '';
      if (value > 0xFFFF) {
        value -= 0x10000;
        output += String.fromCharCode(value >>> 10 & 0x3FF | 0xD800);
        value = 0xDC00 | value & 0x3FF;
      }
      output += String.fromCharCode(value);
      return output;
    },

    addEventListener: function(type, listener, useCapture) {
        if (typeof window !== 'undefined') {
            window.addEventListener(type, listener, useCapture);
        }
    },

    removeEventListener: function(type, listener, useCapture) {
        if (typeof window !== 'undefined') {
            window.removeEventListener(type, listener, useCapture);
        }
    },

    addEventListenerCanvas:function (type, listener, useCapture) {
          if (typeof Module['canvas'] !== 'undefined') {
              Module['canvas'].addEventListener(type, listener, useCapture);
          }
      },

    removeEventListenerCanvas:function (type, listener, useCapture) {
        if (typeof Module['canvas'] !== 'undefined') {
            Module['canvas'].removeEventListener(type, listener, useCapture);
        }
    },

    isCanvasActive: function(event) {
      var res = (typeof document.activeElement == 'undefined' || document.activeElement == Module["canvas"]);

      if (!res) {
        res = (event.target == Module["canvas"]);
      }

      // Pass along focus to element that the event was meant for.
      // Chrome on Android (and perhaps more mobile browsers) does not
      // seem to set the document.activeElement, at least while we call
      // event.preventDefault, meaning if the fullscreen button has a
      // click handler it will never be called since the element would
      // never be "active".
      if (event.target.focus)
        event.target.focus();

      return res;
    },

    onKeyPress: function(event) {
      if (!GLFW.isCanvasActive(event)) { return; }

      // charCode is only available whith onKeyPress event
      if (event.charCode) {
        var char = GLFW.getUnicodeChar(event.charCode);
        if (char !== null && GLFW.charFunc) {
          {{{ makeDynCall('vii', 'GLFW.charFunc') }}}(event.charCode, 1);
        }
      }
    },

    onKeyChanged: function(event, status) {
      if (!GLFW.isCanvasActive(event)) { return; }

      var key = GLFW.DOMToGLFWKeyCode(event.keyCode, event.code);
      if (key) {
        GLFW.keys[key] = status;
        if (GLFW.keyFunc) {
          {{{ makeDynCall('vii', 'GLFW.keyFunc') }}}(key, status);
        }
      }
    },

    onKeydown: function(event) {
      if (!GLFW.isCanvasActive(event)) { return; }

      // prevent navigation within the page using arrow keys and space
      switch(event.keyCode) {
        case 37: case 38: case 39:  case 40: // Arrow keys
        case 32: event.preventDefault(); event.stopPropagation(); // Space
        default: break; // do not block other keys
      }


      GLFW.onKeyChanged(event, 1);// GLFW_PRESS
      if (event.keyCode === 32) {
        if (GLFW.charFunc) {
          {{{ makeDynCall('vii', 'GLFW.charFunc') }}}(32, 1);
          event.preventDefault();
        }
      }
      // This logic comes directly from the sdl implementation. We cannot
      // call preventDefault on all keydown events otherwise onKeyPress will
      // not get called
      else if (event.keyCode === 8 /* backspace */ || event.keyCode === 9 /* tab */ || event.keyCode === 13 /* enter */) {
        event.preventDefault();
      }
    },

    onKeyup: function(event) {
      if (!GLFW.isCanvasActive(event)) { return; }

      GLFW.onKeyChanged(event, 0);// GLFW_RELEASE
    },

    onMousemove: function(event) {
      /* Send motion event only if the motion changed, prevents
       * spamming our app with uncessary callback call. It does happen in
       * Chrome on Windows.
       */
      var lastX = Browser.mouseX;
      var lastY = Browser.mouseY;
      Browser.calculateMouseEvent(event);
      var newX = Browser.mouseX;
      var newY = Browser.mouseY;

      if (event.target == Module["canvas"] && GLFW.mousePosFunc) {
        event.preventDefault();
        {{{ makeDynCall('vii', 'GLFW.mousePosFunc') }}}(lastX, lastY);
      }
    },

    onMouseButtonChanged: function(event, status) {
      if (!GLFW.isCanvasActive(event)) { return; }

      if (GLFW.mouseButtonFunc == null) {
        return;
      }

      Browser.calculateMouseEvent(event);

      if (event.target != Module["canvas"]) {
        return;
      }

      if (status == 1) {// GLFW_PRESS
        try {
          event.target.setCapture();
        } catch (e) {}
      }

      event.preventDefault();

      // DOM and glfw have different button codes
      var eventButton = GLFW.DOMtoGLFWButton(event['button']);

      {{{ makeDynCall('vii', 'GLFW.mouseButtonFunc') }}}(eventButton, status);
    },

    fillTouch: function(id, x, y, phase) {
      if (GLFW.touchFunc) {
        {{{ makeDynCall('viiii', 'GLFW.touchFunc') }}}(id, x, y, phase);
      }
    },

    touchWasFinished: function(event, phase) {
        if (!GLFW.isCanvasActive(event)) { return; }

        for(var i = 0; i < event.changedTouches.length; ++i) {
          var touch = event.changedTouches[i];
          var coord = GLFW.convertCoordinatesFromMonitorToWebGLPixels(touch.clientX, touch.clientY);
          var canvasX = coord[0];
          var canvasY = coord[1];
          GLFW.fillTouch(touch.identifier, canvasX, canvasY, phase);
          if (touch.identifier == GLFW.mouseTouchId) {
              GLFW.mouseTouchId = null;
              GLFW.buttons &= ~(1 << 0);
            }
        }

        if (event.touches.length == 0){
            GLFW.buttons &= ~(1 << 0);
        }
        // Audio is blocked by default in some browsers until a user performs an interaction,
        // so we need to try to resume it here (on mouse button up and touch end).
        // We must also check that the sound device hasn't been stripped
        if ((typeof DefoldSoundDevice != "undefined") && (DefoldSoundDevice != null)) {
            DefoldSoundDevice.TryResumeAudio();
        }

        event.preventDefault();
    },

    onTouchEnd: function(event) {
      GLFW.touchWasFinished(event, GLFW.GLFW_PHASE_ENDED);
    },

    onTouchCancel: function(event) {
      GLFW.touchWasFinished(event, GLFW.GLFW_PHASE_CANCELLED);
    },

    convertCoordinatesFromMonitorToWebGLPixels: function(x,y) {
        var rect = Module['canvas'].getBoundingClientRect();
        var canvasWidth = rect.right - rect.left;
        var canvasHeight = rect.bottom - rect.top;

        var canvasX = x - rect.left;
        var canvasY = y - rect.top;

        var canvasXNormalized = canvasX / canvasWidth;
        var canvasYNormalized = canvasY / canvasHeight;

        var finalX = Module['canvas'].width * canvasXNormalized;
        var finalY = Module['canvas'].height * canvasYNormalized;
        return [finalX, finalY];
    },

    onTouchMove: function(event) {
        if (!GLFW.isCanvasActive(event)) { return; }

        var e = event;
        var touch;
        var coord;
        var canvasX;
        var canvasY
        for(var i = 0; i < e.changedTouches.length; ++i) {
          touch = e.changedTouches[i];
          coord = GLFW.convertCoordinatesFromMonitorToWebGLPixels(touch.clientX, touch.clientY);
          canvasX = coord[0];
          canvasY = coord[1];
          if (touch.identifier == GLFW.mouseTouchId) {
            Browser.mouseX = canvasX;
            Browser.mouseY = canvasY;
          }
          GLFW.fillTouch(touch.identifier, canvasX, canvasY, GLFW.GLFW_PHASE_MOVED);
        }

        event.preventDefault();
    },

    onTouchStart: function(event) {
        // We don't check if canvas is active here, instead
        // check if the target is the canvas directly.
        if (event.target != Module["canvas"]) { return; }

        var e = event;
        var touch;
        var coord;
        var canvasX;
        var canvasY
        for(var i = 0; i < e.changedTouches.length; ++i) {
          touch = e.changedTouches[i];
          coord = GLFW.convertCoordinatesFromMonitorToWebGLPixels(touch.clientX, touch.clientY);
          canvasX = coord[0];
          canvasY = coord[1];
          if (i == 0 && GLFW.mouseTouchId == null) {
            GLFW.mouseTouchId = touch.identifier;
            GLFW.buttons |= (1 << 0);
            Browser.mouseX = canvasX;
            Browser.mouseY = canvasY;
          }
          GLFW.fillTouch(touch.identifier, canvasX, canvasY, GLFW.GLFW_PHASE_BEGAN);
        }

        event.preventDefault();
    },

    onMouseButtonDown: function(event) {
      // We don't check if canvas is active here, instead
      // check if the target is the canvas directly.
      if (event.target != Module["canvas"]) { return; }

      GLFW.buttons |= (1 << event['button']);
      GLFW.onMouseButtonChanged(event, 1);// GLFW_PRESS
    },

    onMouseButtonUp: function(event) {
      if (!GLFW.isCanvasActive(event)) { return; }

      GLFW.buttons &= ~(1 << event['button']);
      GLFW.onMouseButtonChanged(event, 0);// GLFW_RELEASE

      // Audio is blocked by default in some browsers until a user performs an interaction,
      // so we need to try to resume it here (on mouse button up and touch end).
      // We must also check that the sound device hasn't been stripped
      if ((typeof DefoldSoundDevice != "undefined") && (DefoldSoundDevice != null)) {
          DefoldSoundDevice.TryResumeAudio();
      }
    },

    onMouseWheel: function(event) {
      if (!GLFW.isCanvasActive(event)) { return; }

      GLFW.wheelPos += Browser.getMouseWheelDelta(event);

      if (event.target == Module["canvas"]) {
        if (GLFW.mouseWheelFunc) {
          {{{ makeDynCall('vi', 'GLFW.mouseWheelFunc') }}}(GLFW.wheelPos);
        }
        if (event.cancelable) {
          event.preventDefault();
        }
      }
    },

    onFocusChanged: function(focus) {
      // If a key is pressed while the game lost focus and that key is released while
      // not in focus the event will not be received for the key release. This will
      // result in the key remaining in the pressed state when the game regains focus.
      // To fix this we set all pressed keys to released when focus is lost.
      if (focus == 0) {
        for (var i = 0; i < GLFW.keys.length; i++) {
          GLFW.keys[i] = 0;
        }
      }
      if (GLFW.focusFunc) {
        {{{ makeDynCall('vi', 'GLFW.focusFunc') }}}(focus);
      }
    },

    onFocus: function(event) {
      GLFW.onFocusChanged(1);
    },

    onBlur: function(event) {
      GLFW.onFocusChanged(0);
    },

    onFullScreenEventChange: function(event) {
      GLFW.isFullscreen = document["fullScreen"] || document["mozFullScreen"] || document["webkitIsFullScreen"] || document["msIsFullScreen"];
      if (!GLFW.isFullscreen) {
        document.removeEventListener('fullscreenchange', GLFW.onFullScreenEventChange, true);
        document.removeEventListener('mozfullscreenchange', GLFW.onFullScreenEventChange, true);
        document.removeEventListener('webkitfullscreenchange', GLFW.onFullScreenEventChange, true);
        document.removeEventListener('msfullscreenchange', GLFW.onFullScreenEventChange, true);
      }
      //reset previous values for updating size in glfwSwapBuffers()
      GLFW.prevWidth = 0;
      GLFW.prevHeight = 0;
    },

    requestFullScreen: function(element) {
      element = element || Module["fullScreenContainer"] || Module["canvas"];
      if (!element) {
        return;
      }
      document.addEventListener('fullscreenchange', GLFW.onFullScreenEventChange, true);
      document.addEventListener('mozfullscreenchange', GLFW.onFullScreenEventChange, true);
      document.addEventListener('webkitfullscreenchange', GLFW.onFullScreenEventChange, true);
      document.addEventListener('msfullscreenchange', GLFW.onFullScreenEventChange, true);
      var RFS = element['requestFullscreen'] ||
                element['requestFullScreen'] ||
                element['mozRequestFullScreen'] ||
                element['webkitRequestFullScreen'] ||
                element['msRequestFullScreen'] ||
                (function() {});
      RFS.apply(element, []);
    },

    cancelFullScreen: function() {
      var CFS = document['exitFullscreen'] ||
                document['cancelFullScreen'] ||
                document['mozCancelFullScreen'] ||
                document['webkitCancelFullScreen'] ||
                document['msExitFullscreen'] ||
          (function() {});
      CFS.apply(document, []);
    },

    onJoystickConnected: function(event) {
      GLFW.refreshJoysticks();
    },

    onJoystickDisconnected: function(event) {
      GLFW.refreshJoysticks(true);
    },

    onPointerLockEventChange: function(event) {
      GLFW.isPointerLocked = !!document["pointerLockElement"];
      if (!GLFW.isPointerLocked) {
        document.removeEventListener('pointerlockchange', GLFW.onPointerLockEventChange, true);
      }
    },

    requestPointerLock: function(element) {
      element = element || Module["canvas"];
      if (!element) {
        return;
      }

      if (!GLFW.isPointerLocked)
      {
        document.addEventListener('pointerlockchange', GLFW.onPointerLockEventChange, true);
        var RPL = element.requestPointerLock || (function() {});
        RPL.apply(element, []);
      }
    },

    cancelPointerLock: function() {
      var EPL = document.exitPointerLock || (function() {});
      EPL.apply(document, []);
    },
    disconnectJoystick: function (joy) {
      _free(GLFW.joys[joy].id);
      delete GLFW.joys[joy];
      if (GLFW.gamepadFunc) {
        {{{ makeDynCall('vii', 'GLFW.gamepadFunc') }}}(joy, 0);
      }
    },

    //adaptation for our needs of https://github.com/emscripten-core/emscripten/blob/941bbc6b9b35d3124f17d2503d7a32cc81032dac/src/library_glfw.js#L662
    joys: {}, // glfw joystick data
    lastGamepadState: null,
    lastGamepadStateFrame: null, // The integer value of Browser.mainLoop.currentFrameNumber of when the last gamepad state was produced.

    refreshJoysticks: function(forceUpdate) {
        // Produce a new Gamepad API sample if we are ticking a new game frame, or if not using emscripten_set_main_loop() at all to drive animation.
        if (forceUpdate || Browser.mainLoop.currentFrameNumber !== GLFW.lastGamepadStateFrame || !Browser.mainLoop.currentFrameNumber) {
          GLFW.lastGamepadState = navigator.getGamepads ? navigator.getGamepads() : (navigator.webkitGetGamepads ? navigator.webkitGetGamepads : null);
          if (!GLFW.lastGamepadState) {
            return;
          }
          GLFW.lastGamepadStateFrame = Browser.mainLoop.currentFrameNumber;
          for (var joy = 0; joy < GLFW.lastGamepadState.length; ++joy) {
            var gamepad = GLFW.lastGamepadState[joy];

            if (gamepad) {
              if (!GLFW.joys[joy] || GLFW.joys[joy].id_string != gamepad.id) {
                if (GLFW.joys[joy]) {
                  //In case when user change gamepad while browser in background (minimized)
                  GLFW.disconnectJoystick(joy);
                }
                GLFW.joys[joy] = {
                  id: allocate(intArrayFromString(gamepad.id), ALLOC_NORMAL),
                  id_string: gamepad.id,
                  axesCount: gamepad.axes.length,
                  buttonsCount: gamepad.buttons.length
                };
                if (GLFW.gamepadFunc) {
                  {{{ makeDynCall('vii', 'GLFW.gamepadFunc') }}}(joy, 1);
                }
              }
              GLFW.joys[joy].buttons = gamepad.buttons;
              GLFW.joys[joy].axes = gamepad.axes;
            } else {
              if (GLFW.joys[joy]) {
                GLFW.disconnectJoystick(joy);
              }
          }
        }
      }
    }
  },

/*******************************************************************************
 * GLFW FUNCTIONS
 ******************************************************************************/

  /* GLFW initialization, termination and version querying */
  glfwInitJS: function() {
    GLFW.initTime = Date.now() / 1000;

    GLFW.addEventListener("gamepadconnected", GLFW.onJoystickConnected, true);
    GLFW.addEventListener("gamepaddisconnected", GLFW.onJoystickDisconnected, true);
    GLFW.addEventListener("keydown", GLFW.onKeydown, true);
    GLFW.addEventListener("keypress", GLFW.onKeyPress, true);
    GLFW.addEventListener("keyup", GLFW.onKeyup, true);
    GLFW.addEventListener("mousemove", GLFW.onMousemove, true);
    GLFW.addEventListener("mousedown", GLFW.onMouseButtonDown, true);
    GLFW.addEventListener("mouseup", GLFW.onMouseButtonUp, true);
    GLFW.addEventListener('DOMMouseScroll', GLFW.onMouseWheel, { capture: true, passive: false });
    GLFW.addEventListener('mousewheel', GLFW.onMouseWheel, { capture: true, passive: false });
    GLFW.addEventListenerCanvas('touchstart', GLFW.onTouchStart, true);
    GLFW.addEventListenerCanvas('touchend', GLFW.onTouchEnd, true);
    GLFW.addEventListenerCanvas('touchcancel', GLFW.onTouchCancel, true);
    GLFW.addEventListenerCanvas('touchmove', GLFW.onTouchMove, true);
    GLFW.addEventListenerCanvas('focus', GLFW.onFocus, true);
    GLFW.addEventListenerCanvas('blur', GLFW.onBlur, true);

    __ATEXIT__.push({ func: function() {
        GLFW.removeEventListener("gamepadconnected", GLFW.onJoystickConnected, true);
        GLFW.removeEventListener("gamepaddisconnected", GLFW.onJoystickDisconnected, true);
        GLFW.removeEventListener("keydown", GLFW.onKeydown, true);
        GLFW.removeEventListener("keypress", GLFW.onKeyPress, true);
        GLFW.removeEventListener("keyup", GLFW.onKeyup, true);
        GLFW.removeEventListener("mousemove", GLFW.onMousemove, true);
        GLFW.removeEventListener("mousedown", GLFW.onMouseButtonDown, true);
        GLFW.removeEventListener("mouseup", GLFW.onMouseButtonUp, true);
        GLFW.removeEventListener('DOMMouseScroll', GLFW.onMouseWheel, { capture: true, passive: false });
        GLFW.removeEventListener('mousewheel', GLFW.onMouseWheel, { capture: true, passive: false });
        GLFW.removeEventListenerCanvas('touchstart', GLFW.onTouchStart, true);
        GLFW.removeEventListenerCanvas('touchend', GLFW.onTouchEnd, true);
        GLFW.removeEventListenerCanvas('touchcancel', GLFW.onTouchEnd, true);
        GLFW.removeEventListenerCanvas('touchmove', GLFW.onTouchMove, true);
        GLFW.removeEventListenerCanvas('focus', GLFW.onFocus, true);
        GLFW.removeEventListenerCanvas('blur', GLFW.onBlur, true);

        var canvas = Module["canvas"];
        if (typeof canvas !== 'undefined') {
            Module["canvas"].width = Module["canvas"].height = 1;
        }
    }});

    //TODO: Init with correct values
    GLFW.params = new Array();
    GLFW.params[0x00030001] = true; // GLFW_MOUSE_CURSOR
    GLFW.params[0x00030002] = false; // GLFW_STICKY_KEYS
    GLFW.params[0x00030003] = true; // GLFW_STICKY_MOUSE_BUTTONS
    GLFW.params[0x00030004] = false; // GLFW_SYSTEM_KEYS
    GLFW.params[0x00030005] = false; // GLFW_KEY_REPEAT
    GLFW.params[0x00030006] = true; // GLFW_AUTO_POLL_EVENTS
    GLFW.params[0x00020001] = true; // GLFW_OPENED
    GLFW.params[0x00020002] = true; // GLFW_ACTIVE
    GLFW.params[0x00020003] = false; // GLFW_ICONIFIED
    GLFW.params[0x00020004] = true; // GLFW_ACCELERATED
    GLFW.params[0x00020005] = 0; // GLFW_RED_BITS
    GLFW.params[0x00020006] = 0; // GLFW_GREEN_BITS
    GLFW.params[0x00020007] = 0; // GLFW_BLUE_BITS
    GLFW.params[0x00020008] = 0; // GLFW_ALPHA_BITS
    GLFW.params[0x00020009] = 0; // GLFW_DEPTH_BITS
    GLFW.params[0x0002000A] = 0; // GLFW_STENCIL_BITS
    GLFW.params[0x0002000B] = 0; // GLFW_REFRESH_RATE
    GLFW.params[0x0002000C] = 0; // GLFW_ACCUM_RED_BITS
    GLFW.params[0x0002000D] = 0; // GLFW_ACCUM_GREEN_BITS
    GLFW.params[0x0002000E] = 0; // GLFW_ACCUM_BLUE_BITS
    GLFW.params[0x0002000F] = 0; // GLFW_ACCUM_ALPHA_BITS
    GLFW.params[0x00020010] = 0; // GLFW_AUX_BUFFERS
    GLFW.params[0x00020011] = 0; // GLFW_STEREO
    GLFW.params[0x00020012] = 0; // GLFW_WINDOW_NO_RESIZE
    GLFW.params[0x00020013] = 0; // GLFW_FSAA_SAMPLES
    GLFW.params[0x00020014] = 0; // GLFW_OPENGL_VERSION_MAJOR
    GLFW.params[0x00020015] = 0; // GLFW_OPENGL_VERSION_MINOR
    GLFW.params[0x00020016] = 0; // GLFW_OPENGL_FORWARD_COMPAT
    GLFW.params[0x00020017] = 0; // GLFW_OPENGL_DEBUG_CONTEXT
    GLFW.params[0x00020018] = 0; // GLFW_OPENGL_PROFILE
    GLFW.params[0x00050001] = 0; // GLFW_PRESENT
    GLFW.params[0x00050002] = 1; // GLFW_AXES
    GLFW.params[0x00050003] = 2; // GLFW_BUTTONS
    GLFW.params[0x00020019] = 0; // GLFW_WINDOW_HIGH_DPI

    GLFW.keys = new Array();

    GLFW.GLFW_PHASE_BEGAN = 0;
    GLFW.GLFW_PHASE_MOVED = 1;
    GLFW.GLFW_PHASE_ENDED = 3;
    GLFW.GLFW_PHASE_CANCELLED = 4;

    return 1; // GL_TRUE
  },

  glfwTerminate: function() {},

  glfwGetVersion: function(major, minor, rev) {
    setValue(major, 2, 'i32');
    setValue(minor, 7, 'i32');
    setValue(rev, 7, 'i32');
  },

  /* Window handling */
  glfwOpenWindow__deps: ['$Browser'],
  glfwOpenWindow: function(width, height, redbits, greenbits, bluebits, alphabits, depthbits, stencilbits, mode) {
    if (width == 0 && height > 0) {
      width = 4 * height / 3;
    }
    if (width > 0 && height == 0) {
      height = 3 * width / 4;
    }
    GLFW.params[0x00020005] = redbits; // GLFW_RED_BITS
    GLFW.params[0x00020006] = greenbits; // GLFW_GREEN_BITS
    GLFW.params[0x00020007] = bluebits; // GLFW_BLUE_BITS
    GLFW.params[0x00020008] = alphabits; // GLFW_ALPHA_BITS
    GLFW.params[0x00020009] = depthbits; // GLFW_DEPTH_BITS
    GLFW.params[0x0002000A] = stencilbits; // GLFW_STENCIL_BITS

    if (mode == 0x00010001) {// GLFW_WINDOW
      GLFW.initWindowWidth = width;
      GLFW.initWindowHeight = height;
      GLFW.params[0x00030003] = true; // GLFW_STICKY_MOUSE_BUTTONS
    } else if (mode == 0x00010002) {// GLFW_FULLSCREEN
      GLFW.requestFullScreen();
      GLFW.params[0x00030003] = false; // GLFW_STICKY_MOUSE_BUTTONS
    } else {
      throw "Invalid glfwOpenWindow mode.";
    }

    var contextAttributes = {
      antialias: (GLFW.params[0x00020013] > 1), // GLFW_FSAA_SAMPLES
      depth: (GLFW.params[0x00020009] > 0), // GLFW_DEPTH_BITS
      stencil: (GLFW.params[0x0002000A] > 0) // GLFW_STENCIL_BITS
    };

    // iOS < 15.2 has issues with WebGl 2.0 contexts. It's created without issues but doesn't work.
    var iOSVersion = false;
    try {
      iOSVersion = parseFloat(('' + (/CPU.*OS ([0-9_]{1,5})|(CPU like).*AppleWebKit.*Mobile/i.exec(navigator.userAgent) || [0,''])[1]) .replace('undefined', '3_2').replace('_', '.').replace('_', '')) || false;
    } catch (e) {}

    if (iOSVersion && iOSVersion < 15.2)
    {
      contextAttributes.majorVersion = 1;
    }

    // Browser.createContext: https://github.com/emscripten-core/emscripten/blob/master/src/library_browser.js#L312
    Module.ctx = Browser.createContext(Module['canvas'], true, true, contextAttributes);
    if (Module.ctx == null) {
      contextAttributes.majorVersion = 1; // Try WebGL 1
      Module.ctx = Browser.createContext(Module['canvas'], true, true, contextAttributes);
    }

    return 1; // GL_TRUE
  },

  glfwOpenWindowHint: function(target, hint) {
    GLFW.params[target] = hint;
    // if display._high_dpi flag is on in game.project
    // we get information about the current pixel ratio from browser
    if (target == 0x00020019) { //GLFW_WINDOW_HIGH_DPI
      if (hint != 0) {
        GLFW.dpi = window.devicePixelRatio || 1;
      }
    }
  },

  glfwCloseWindow__deps: ['$Browser'],
  glfwCloseWindow: function() {
    if (GLFW.closeFunc) {
      {{{ makeDynCall('i', 'GLFW.closeFunc') }}}();
    }
    Module.ctx = Browser.destroyContext(Module['canvas'], true, true);
  },

  glfwSetWindowTitle: function(title) {
    document.title = UTF8ToString(title);
  },

  glfwGetWindowSize: function(width, height) {
    setValue(width, Module['canvas'].width, 'i32');
    setValue(height, Module['canvas'].height, 'i32');
  },

  glfwSetWindowSize: function(width, height) {
      Browser.setCanvasSize(width, height);
      if (GLFW.resizeFunc) {
        {{{ makeDynCall('vii', 'GLFW.resizeFunc') }}}(width, height);
      }
  },

  glfwSetWindowPos: function(x, y) {},

  glfwIconifyWindow: function() {},

  glfwRestoreWindow: function() {},

  glfwSwapBuffers__deps: ['glfwSetWindowSize'],
  glfwSwapBuffers: function() {

    var width = Module['canvas'].width;
    var height = Module['canvas'].height;

    if (GLFW.prevWidth != width || GLFW.prevHeight != height) {
      if (GLFW.isFullscreen) {
        width = Math.floor(window.innerWidth * GLFW.dpi);
        height = Math.floor(window.innerHeight * GLFW.dpi);
      } else {
        width = Math.floor(width * GLFW.dpi);
        height = Math.floor(height * GLFW.dpi);
      }
      GLFW.prevWidth = width;
      GLFW.prevHeight = height;
      _glfwSetWindowSize(width, height);
    }
  },

  glfwSwapInterval: function(interval) {},

  glfwGetWindowParam: function(param) {
    return GLFW.params[param];
  },

  glfwSetWindowSizeCallback: function(cbfun) {
    GLFW.resizeFunc = cbfun;
  },

  glfwSetWindowCloseCallback: function(cbfun) {
    GLFW.closeFunc = cbfun;
  },

  glfwSetWindowRefreshCallback: function(cbfun) {
    GLFW.refreshFunc = cbfun;
  },

  glfwSetWindowFocusCallback: function(cbfun) {
    GLFW.focusFunc = cbfun;
  },

  glfwSetWindowIconifyCallback: function(cbfun) {
    GLFW.iconifyFunc = cbfun;
  },

  /* Video mode functions */
  glfwGetVideoModes: function(list, maxcount) { throw "glfwGetVideoModes is not implemented."; },

  glfwGetDesktopMode: function(mode) { throw "glfwGetDesktopMode is not implemented."; },

  /* Input handling */
  glfwPollEvents: function() {},

  glfwWaitEvents: function() {},

  glfwGetKey: function(key) {
    return GLFW.keys[key];
  },

  glfwGetMouseButton: function(button) {
    return (GLFW.buttons & (1 << GLFW.DOMtoGLFWButton(button))) > 0;
  },

  glfwGetMousePos: function(xpos, ypos) {
    setValue(xpos, Browser.mouseX, 'i32');
    setValue(ypos, Browser.mouseY, 'i32');
  },

  // I believe it is not possible to move the mouse with javascript
  glfwSetMousePos: function(xpos, ypos) {},

  glfwGetMouseWheel: function() {
    return GLFW.wheelPos;
  },

  glfwSetMouseWheel: function(pos) {
    GLFW.wheelPos = pos;
  },

  glfwGetMouseLocked: function() {
    return GLFW.isPointerLocked ? 1 : 0;
  },

  glfwSetKeyCallback: function(cbfun) {
    GLFW.keyFunc = cbfun;
  },

  glfwSetCharCallback: function(cbfun) {
    GLFW.charFunc = cbfun;
    return 1;
  },

  glfwSetMarkedTextCallback: function(cbfun) {
    GLFW.markedTextFunc = cbfun;
    return 1;
  },

  glfwSetMouseButtonCallback: function(cbfun) {
    GLFW.mouseButtonFunc = cbfun;
  },

  glfwSetMousePosCallback: function(cbfun) {
    GLFW.mousePosFunc = cbfun;
  },

  glfwSetMouseWheelCallback: function(cbfun) {
    GLFW.mouseWheelFunc = cbfun;
  },

  glfwSetGamepadCallback: function(cbfun) {
    GLFW.gamepadFunc = cbfun;
    GLFW.refreshJoysticks();
    return 1;
  },

  /* Joystick input */

  glfwGetJoystickParam: function(joy, param) {
    var result = 0; //GL_FALSE
    if (GLFW.joys[joy]) {
      switch (GLFW.params[param]) {
        case 0: // GLFW_PRESENT
          result = 1; //GL_TRUE
          break;
        case 1: // GLFW_AXES
          result = GLFW.joys[joy].axesCount;
          break;
        case 2: // GLFW_BUTTONS
          result = GLFW.joys[joy].buttonsCount;
          break;
        }
    }
    return result;
  },

  glfwGetJoystickPos: function(joy, pos, numaxes) {
    GLFW.refreshJoysticks();
    var state = GLFW.joys[joy];
    if (!state || !state.axes) {
      for (var i = 0; i < numaxes; i++) {
        setValue(pos + i*4, 0, 'float');
      }
      return;
    }

    for (var i = 0; i < numaxes; i++) {
      setValue(pos + i*4, state.axes[i], 'float');
    }
  },

  glfwGetJoystickButtons: function(joy, buttons, numbuttons) {
    GLFW.refreshJoysticks();
    var state = GLFW.joys[joy];
    if (!state || !state.buttons) {
      for (var i = 0; i < numbuttons; i++) {
        setValue(buttons + i, 0, 'i8');
      }
      return;
    }
    for (var i = 0; i < Math.min(numbuttons, state.buttonsCount) ; i++) {
      setValue(buttons + i, state.buttons[i].pressed, 'i8');
    }
  },

  glfwGetJoystickHats: function(joy, buttons, numhats) {
    return 0;
  },

  glfwGetJoystickDeviceId: function(joy, device_id) {
    if (GLFW.joys[joy]) {
      setValue(device_id, GLFW.joys[joy].id, '*');
      return 1;
    } else {
      return 0;
    }
  },

  /* Time */
  glfwGetTime: function() {
    return (Date.now()/1000) - GLFW.initTime;
  },

  glfwSetTime: function(time) {
    GLFW.initTime = Date.now()/1000 + time;
  },

  glfwSleep__deps: ['sleep'],
  glfwSleep: function(time) {
    _sleep(time);
  },

  /* Extension support */
  glfwExtensionSupported: function(extension) {
    return Module.ctx.getSupportedExtensions().indexOf(UTF8ToString(extension)) > -1;
  },

  glfwGetProcAddress__deps: ['glfwGetProcAddress'],
  glfwGetProcAddress: function(procname) {
    return _getProcAddress(procname);
  },

  glfwGetGLVersion: function(major, minor, rev) {
    setValue(major, 0, 'i32');
    setValue(minor, 0, 'i32');
    setValue(rev, 1, 'i32');
  },

  /* Threading support */
  glfwCreateThread: function(fun, arg) {
    {{{ makeDynCall('vi', 'str') }}}(fun, arg); // from emscripten-core 2.0.10
    // One single thread
    return 0;
  },

  glfwDestroyThread: function(ID) {},

  glfwWaitThread: function(ID, waitmode) {},

  glfwGetThreadID: function() {
    // One single thread
    return 0;
  },

  glfwCreateMutex: function() { throw "glfwCreateMutex is not implemented."; },

  glfwDestroyMutex: function(mutex) { throw "glfwDestroyMutex is not implemented."; },

  glfwLockMutex: function(mutex) { throw "glfwLockMutex is not implemented."; },

  glfwUnlockMutex: function(mutex) { throw "glfwUnlockMutex is not implemented."; },

  glfwCreateCond: function() { throw "glfwCreateCond is not implemented."; },

  glfwDestroyCond: function(cond) { throw "glfwDestroyCond is not implemented."; },

  glfwWaitCond: function(cond, mutex, timeout) { throw "glfwWaitCond is not implemented."; },

  glfwSignalCond: function(cond) { throw "glfwSignalCond is not implemented."; },

  glfwBroadcastCond: function(cond) { throw "glfwBroadcastCond is not implemented."; },

  glfwGetNumberOfProcessors: function() {
    // Threads are disabled anyway…
    return 1;
  },

  /* Enable/disable functions */
  glfwEnable: function(token) {
    GLFW.params[token] = true;

    if (token == 0x00030001) // GLFW_MOUSE_CURSOR)
    {
      GLFW.cancelPointerLock();
    }
  },

  glfwDisable: function(token) {
    GLFW.params[token] = false;
    if (token == 0x00030001) // GLFW_MOUSE_CURSOR)
    {
      GLFW.requestPointerLock();
    }
  },

  /* Image/texture I/O support */
  glfwReadImage: function(name, img, flags) { throw "glfwReadImage is not implemented."; },

  glfwReadMemoryImage: function(data, size, img, flags) { throw "glfwReadMemoryImage is not implemented."; },

  glfwFreeImage: function(img) { throw "glfwFreeImage is not implemented."; },

  glfwLoadTexture2D: function(name, flags) { throw "glfwLoadTexture2D is not implemented."; },

  glfwLoadMemoryTexture2D: function(data, size, flags) { throw "glfwLoadMemoryTexture2D is not implemented."; },

  glfwLoadTextureImage2D: function(img, flags) { throw "glfwLoadTextureImage2D is not implemented."; },

  glfwShowKeyboard: function(show_keyboard) {
    Module['canvas'].contentEditable = show_keyboard ? true : false;
    if (show_keyboard) {
      Module['canvas'].focus();
    }
  },

  glfwResetKeyboard: function() {
  },

  glfwSetTouchCallback: function(cbfun) {
    GLFW.touchFunc = cbfun;
    return 1;
  },

  glfwGetAcceleration: function(x, y, z) {
      return 0;
  },

  glfwGetWindowRefreshRate: function() {
    return 0;
  },

  glfwGetDefaultFramebuffer: function() {
	  return 0;
  },

  glfwGetNativeHandles: function() {
    return 0;
  },

  glfwAccelerometerEnable: function() {
  },

  glfwSetWindowBackgroundColor: function() {
  },

  glfwGetDisplayScaleFactor: function() {
    return 1;
  }
};

autoAddDeps(LibraryGLFW, '$GLFW');
mergeInto(LibraryManager.library, LibraryGLFW);
