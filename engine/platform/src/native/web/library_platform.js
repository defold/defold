// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

// Derived from the Emscripten GLFW 2 implementation by
// Éloi Rivard <eloi.rivard@gmail.com> and Thomas Borsos <thomasborsos@gmail.com>.
// Modified for Defold's native browser lifecycle, input and canvas backend.

var LibraryDefoldPlatform = {
  $DefoldPlatform: {

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

    DOMToNativeKeyCode: function(keycode, code) {
      switch (keycode) {
        case 0x08: return 295 ; // DOM_VK_BACKSPACE -> NATIVE_KEY_BACKSPACE
        case 0x09: return 293 ; // DOM_VK_TAB -> NATIVE_KEY_TAB
        case 0x0D: return 294 ; // DOM_VK_ENTER -> NATIVE_KEY_ENTER
        case 0x1B: return 257 ; // DOM_VK_ESCAPE -> NATIVE_KEY_ESC
        case 0x6A: return 313 ; // DOM_VK_MULTIPLY -> NATIVE_KEY_KP_MULTIPLY
        case 0x6B: return 315 ; // DOM_VK_ADD -> NATIVE_KEY_KP_ADD
        case 0x6D: return 314 ; // DOM_VK_SUBTRACT -> NATIVE_KEY_KP_SUBTRACT
        case 0x6E: return 316 ; // DOM_VK_DECIMAL -> NATIVE_KEY_KP_DECIMAL
        case 0x6F: return 312 ; // DOM_VK_DIVIDE -> NATIVE_KEY_KP_DIVIDE
        case 0x70: return 258 ; // DOM_VK_F1 -> NATIVE_KEY_F1
        case 0x71: return 259 ; // DOM_VK_F2 -> NATIVE_KEY_F2
        case 0x72: return 260 ; // DOM_VK_F3 -> NATIVE_KEY_F3
        case 0x73: return 261 ; // DOM_VK_F4 -> NATIVE_KEY_F4
        case 0x74: return 262 ; // DOM_VK_F5 -> NATIVE_KEY_F5
        case 0x75: return 263 ; // DOM_VK_F6 -> NATIVE_KEY_F6
        case 0x76: return 264 ; // DOM_VK_F7 -> NATIVE_KEY_F7
        case 0x77: return 265 ; // DOM_VK_F8 -> NATIVE_KEY_F8
        case 0x78: return 266 ; // DOM_VK_F9 -> NATIVE_KEY_F9
        case 0x79: return 267 ; // DOM_VK_F10 -> NATIVE_KEY_F10
        case 0x7a: return 268 ; // DOM_VK_F11 -> NATIVE_KEY_F11
        case 0x7b: return 269 ; // DOM_VK_F12 -> NATIVE_KEY_F12
        case 0x25: return 285 ; // DOM_VK_LEFT -> NATIVE_KEY_LEFT
        case 0x26: return 283 ; // DOM_VK_UP -> NATIVE_KEY_UP
        case 0x27: return 286 ; // DOM_VK_RIGHT -> NATIVE_KEY_RIGHT
        case 0x28: return 284 ; // DOM_VK_DOWN -> NATIVE_KEY_DOWN
        case 0x21: return 298 ; // DOM_VK_PAGE_UP -> NATIVE_KEY_PAGEUP
        case 0x22: return 299 ; // DOM_VK_PAGE_DOWN -> NATIVE_KEY_PAGEDOWN
        case 0x24: return 300 ; // DOM_VK_HOME -> NATIVE_KEY_HOME
        case 0x23: return 301 ; // DOM_VK_END -> NATIVE_KEY_END
        case 0x2d: return 296 ; // DOM_VK_INSERT -> NATIVE_KEY_INSERT
        case 0x2E: return 297 ; // DOM_VK_DEL -> NATIVE_KEY_DEL
        case 16  : return 287 ; // DOM_VK_SHIFT -> NATIVE_KEY_LSHIFT
        case 0x05: return 287 ; // DOM_VK_LEFT_SHIFT -> NATIVE_KEY_LSHIFT
        case 0x06: return 288 ; // DOM_VK_RIGHT_SHIFT -> NATIVE_KEY_RSHIFT
        case 17  : return 289 ; // DOM_VK_CONTROL -> NATIVE_KEY_LCTRL
        case 0x03: return 289 ; // DOM_VK_LEFT_CONTROL -> NATIVE_KEY_LCTRL
        case 0x04: return 290 ; // DOM_VK_RIGHT_CONTROL -> NATIVE_KEY_RCTRL
        case 18  : return 291 ; // DOM_VK_ALT -> NATIVE_KEY_LALT
        case 0x02: return 291 ; // DOM_VK_LEFT_ALT -> NATIVE_KEY_LALT
        case 0x01: return 292 ; // DOM_VK_RIGHT_ALT -> NATIVE_KEY_RALT
        case 96  : return 302 ; // NATIVE_KEY_KP_0
        case 97  : return 303 ; // NATIVE_KEY_KP_1
        case 98  : return 304 ; // NATIVE_KEY_KP_2
        case 99  : return 305 ; // NATIVE_KEY_KP_3
        case 100 : return 306 ; // NATIVE_KEY_KP_4
        case 101 : return 307 ; // NATIVE_KEY_KP_5
        case 102 : return 308 ; // NATIVE_KEY_KP_6
        case 103 : return 309 ; // NATIVE_KEY_KP_7
        case 104 : return 310 ; // NATIVE_KEY_KP_8
        case 105 : return 311 ; // NATIVE_KEY_KP_9
      }

      // Map additional keys not already mapped to any Native keys
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
        case "NumpadComma":   return 316 ; // NATIVE_KEY_KP_DECIMAL, https://www.w3.org/TR/uievents-code/#keyboard-104
      }

      return keycode;
    },

    // The button ids for right and middle are swapped between Native and JS.
    // JS: right = 2, middle = 1
    // Native: right = 1, middle = 2
    // Use this function to convert between JS and Native, and back.
    DOMtoNativeButton: function(button) {
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

    onWindowClose: function(event) {
        DefoldPlatform.params[1] = false; // NATIVE_OPENED
    },

    onKeyPress: function(event) {
      if (!DefoldPlatform.isCanvasActive(event)) { return; }

      // charCode is only available whith onKeyPress event
      if (event.charCode) {
        var char = DefoldPlatform.getUnicodeChar(event.charCode);
        if (char !== null && DefoldPlatform.charFunc) {
          {{{ makeDynCall('vii', 'DefoldPlatform.charFunc') }}}(event.charCode, 1);
        }
      }
    },

    onKeyChanged: function(event, status) {
      if (!DefoldPlatform.isCanvasActive(event)) { return; }

      var key = DefoldPlatform.DOMToNativeKeyCode(event.keyCode, event.code);
      if (key) {
        DefoldPlatform.keys[key] = status;
        if (DefoldPlatform.keyFunc) {
          {{{ makeDynCall('vii', 'DefoldPlatform.keyFunc') }}}(key, status);
        }
      }
    },

    onKeydown: function(event) {
      if (!DefoldPlatform.isCanvasActive(event)) { return; }

      // prevent navigation within the page using arrow keys and space
      switch(event.keyCode) {
        case 37: case 38: case 39:  case 40: // Arrow keys
        case 32: event.preventDefault(); event.stopPropagation(); // Space
        default: break; // do not block other keys
      }


      DefoldPlatform.onKeyChanged(event, 1);// NATIVE_PRESS
      if (event.keyCode === 32) {
        if (DefoldPlatform.charFunc) {
          {{{ makeDynCall('vii', 'DefoldPlatform.charFunc') }}}(32, 1);
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
      if (!DefoldPlatform.isCanvasActive(event)) { return; }

      DefoldPlatform.onKeyChanged(event, 0);// NATIVE_RELEASE
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

      if (event.target == Module["canvas"] && DefoldPlatform.mousePosFunc) {
        event.preventDefault();
        {{{ makeDynCall('vii', 'DefoldPlatform.mousePosFunc') }}}(lastX, lastY);
      }
    },

    onMouseButtonChanged: function(event, status) {
      if (!DefoldPlatform.isCanvasActive(event)) { return; }

      if (DefoldPlatform.mouseButtonFunc == null) {
        return;
      }

      Browser.calculateMouseEvent(event);

      if (event.target != Module["canvas"]) {
        return;
      }

      if (status == 1) {// NATIVE_PRESS
        try {
          event.target.setCapture();
        } catch (e) {}
      }

      event.preventDefault();

      // DOM and dmNative have different button codes
      var eventButton = DefoldPlatform.DOMtoNativeButton(event['button']);

      {{{ makeDynCall('vii', 'DefoldPlatform.mouseButtonFunc') }}}(eventButton, status);
    },

    fillTouch: function(id, x, y, phase) {
      if (DefoldPlatform.touchFunc) {
        {{{ makeDynCall('viiii', 'DefoldPlatform.touchFunc') }}}(id, x, y, phase);
      }
    },

    touchWasFinished: function(event, phase) {
        if (!DefoldPlatform.isCanvasActive(event)) { return; }

        for(var i = 0; i < event.changedTouches.length; ++i) {
          var touch = event.changedTouches[i];
          var coord = DefoldPlatform.convertCoordinatesFromMonitorToWebGLPixels(touch.clientX, touch.clientY);
          var canvasX = coord[0];
          var canvasY = coord[1];
          DefoldPlatform.fillTouch(touch.identifier, canvasX, canvasY, phase);
          if (touch.identifier == DefoldPlatform.mouseTouchId) {
              DefoldPlatform.mouseTouchId = null;
              DefoldPlatform.buttons &= ~(1 << 0);
            }
        }

        if (event.touches.length == 0){
            DefoldPlatform.buttons &= ~(1 << 0);
        }

        // Audio is blocked by default in browsers until a user performs an interaction,
        // so we need to try to resume on mouse button down/up and touch start/end.
        // We must also check that the sound device hasn't been stripped
        if ((typeof DefoldSoundDevice != "undefined") && (DefoldSoundDevice != null)) {
          DefoldSoundDevice.TryResumeAudio();
        }

        event.preventDefault();
    },

    onTouchEnd: function(event) {
      DefoldPlatform.touchWasFinished(event, DefoldPlatform.NATIVE_PHASE_ENDED);
    },

    onTouchCancel: function(event) {
      DefoldPlatform.touchWasFinished(event, DefoldPlatform.NATIVE_PHASE_CANCELLED);
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
        if (!DefoldPlatform.isCanvasActive(event)) { return; }

        var e = event;
        var touch;
        var coord;
        var canvasX;
        var canvasY
        for(var i = 0; i < e.changedTouches.length; ++i) {
          touch = e.changedTouches[i];
          coord = DefoldPlatform.convertCoordinatesFromMonitorToWebGLPixels(touch.clientX, touch.clientY);
          canvasX = coord[0];
          canvasY = coord[1];
          if (touch.identifier == DefoldPlatform.mouseTouchId) {
            Browser.mouseX = canvasX;
            Browser.mouseY = canvasY;
          }
          DefoldPlatform.fillTouch(touch.identifier, canvasX, canvasY, DefoldPlatform.NATIVE_PHASE_MOVED);
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
          coord = DefoldPlatform.convertCoordinatesFromMonitorToWebGLPixels(touch.clientX, touch.clientY);
          canvasX = coord[0];
          canvasY = coord[1];
          if (i == 0 && DefoldPlatform.mouseTouchId == null) {
            DefoldPlatform.mouseTouchId = touch.identifier;
            DefoldPlatform.buttons |= (1 << 0);
            Browser.mouseX = canvasX;
            Browser.mouseY = canvasY;
          }
          DefoldPlatform.fillTouch(touch.identifier, canvasX, canvasY, DefoldPlatform.NATIVE_PHASE_BEGAN);
        }

        // Resume audio on user interaction (see explanation in touchWasFinished).
        if ((typeof DefoldSoundDevice != "undefined") && (DefoldSoundDevice != null)) {
          DefoldSoundDevice.TryResumeAudio();
        }

        event.preventDefault();
    },

    onMouseButtonDown: function(event) {
      // We don't check if canvas is active here, instead
      // check if the target is the canvas directly.
      if (event.target != Module["canvas"]) { return; }

      DefoldPlatform.buttons |= (1 << event['button']);
      DefoldPlatform.onMouseButtonChanged(event, 1);// NATIVE_PRESS

      // Resume audio on user interaction (see explanation in touchWasFinished).
      if ((typeof DefoldSoundDevice != "undefined") && (DefoldSoundDevice != null)) {
        DefoldSoundDevice.TryResumeAudio();
      }
    },

    onMouseButtonUp: function(event) {
      if (!DefoldPlatform.isCanvasActive(event)) { return; }

      DefoldPlatform.buttons &= ~(1 << event['button']);
      DefoldPlatform.onMouseButtonChanged(event, 0);// NATIVE_RELEASE

      // Resume audio on user interaction (see explanation in touchWasFinished).
      if ((typeof DefoldSoundDevice != "undefined") && (DefoldSoundDevice != null)) {
        DefoldSoundDevice.TryResumeAudio();
      }
    },

    onMouseWheel: function(event) {
      if (!DefoldPlatform.isCanvasActive(event)) { return; }

      DefoldPlatform.wheelPos += Browser.getMouseWheelDelta(event);

      if (event.target == Module["canvas"]) {
        if (DefoldPlatform.mouseWheelFunc) {
          {{{ makeDynCall('vi', 'DefoldPlatform.mouseWheelFunc') }}}(DefoldPlatform.wheelPos);
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
        for (var i = 0; i < DefoldPlatform.keys.length; i++) {
          DefoldPlatform.keys[i] = 0;
        }
        DefoldPlatform.buttons = 0;
      }
      if (DefoldPlatform.focusFunc) {
        {{{ makeDynCall('vi', 'DefoldPlatform.focusFunc') }}}(focus);
      }
    },

    onFocus: function(event) {
      DefoldPlatform.onFocusChanged(1);
    },

    onBlur: function(event) {
      DefoldPlatform.onFocusChanged(0);
    },

    isDocumentFullscreen: function() {
      return !!(document["fullscreenElement"] || document["fullScreen"] || document["mozFullScreen"] || document["webkitIsFullScreen"] || document["msIsFullScreen"]);
    },

    addFullScreenEventListeners: function() {
      document.addEventListener('fullscreenchange', DefoldPlatform.onFullScreenEventChange, true);
      document.addEventListener('mozfullscreenchange', DefoldPlatform.onFullScreenEventChange, true);
      document.addEventListener('webkitfullscreenchange', DefoldPlatform.onFullScreenEventChange, true);
      document.addEventListener('msfullscreenchange', DefoldPlatform.onFullScreenEventChange, true);
    },

    removeFullScreenEventListeners: function() {
      document.removeEventListener('fullscreenchange', DefoldPlatform.onFullScreenEventChange, true);
      document.removeEventListener('mozfullscreenchange', DefoldPlatform.onFullScreenEventChange, true);
      document.removeEventListener('webkitfullscreenchange', DefoldPlatform.onFullScreenEventChange, true);
      document.removeEventListener('msfullscreenchange', DefoldPlatform.onFullScreenEventChange, true);
    },

    onFullScreenEventChange: function(event) {
      DefoldPlatform.isFullscreen = DefoldPlatform.isDocumentFullscreen();
      if (!DefoldPlatform.isFullscreen) {
        DefoldPlatform.removeFullScreenEventListeners();
      }
      //reset previous values for updating size in dmNativeSwapBuffers()
      DefoldPlatform.prevWidth = 0;
      DefoldPlatform.prevHeight = 0;
    },

    requestFullScreen: function(element) {
      element = element || Module["fullScreenContainer"] || Module["canvas"];
      if (!element) {
        return;
      }
      DefoldPlatform.addFullScreenEventListeners();
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
      DefoldPlatform.refreshJoysticks();
    },

    onJoystickDisconnected: function(event) {
      DefoldPlatform.refreshJoysticks(true);
    },

    onPointerLockEventChange: function(event) {
      DefoldPlatform.isPointerLocked = !!document["pointerLockElement"];
      if (!DefoldPlatform.isPointerLocked) {
        document.removeEventListener('pointerlockchange', DefoldPlatform.onPointerLockEventChange, true);
      }
    },

    requestPointerLock: function(element) {
      element = element || Module["canvas"];
      if (!element) {
        return;
      }

      if (!DefoldPlatform.isPointerLocked)
      {
        document.addEventListener('pointerlockchange', DefoldPlatform.onPointerLockEventChange, true);
        var RPL = element.requestPointerLock || (function() {});
        RPL.apply(element, []);
      }
    },

    cancelPointerLock: function() {
      var EPL = document.exitPointerLock || (function() {});
      EPL.apply(document, []);
    },

    updateCRC16: function(crc, string) {
      for (var i = 0; i < string.length; ++i) {
        var r = (crc ^ (string.charCodeAt(i) & 0xff)) & 0xff;
        var byte_crc = 0;

        for (var bit = 0; bit < 8; ++bit) {
          byte_crc = (((byte_crc ^ r) & 1) ? 0xA001 : 0) ^ (byte_crc >> 1);
          r >>= 1;
        }

        crc = (byte_crc ^ (crc >> 8)) & 0xffff;
      }

      return crc;
    },

    writeGUID16: function(guid, offset, value) {
      guid[offset] = value & 0xff;
      guid[offset + 1] = (value >> 8) & 0xff;
    },

    formatGUID: function(guid) {
      var hex = "0123456789abcdef";
      var result = "";
      for (var i = 0; i < guid.length; ++i) {
        result += hex[(guid[i] >> 4) & 0x0f];
        result += hex[guid[i] & 0x0f];
      }
      return result;
    },

    createJoystickGUID: function(vendor, product, name, is_xinput) {
      var guid = new Array(16).fill(0);
      var crc = DefoldPlatform.updateCRC16(0, name);

      DefoldPlatform.writeGUID16(guid, 0, 0);
      DefoldPlatform.writeGUID16(guid, 2, crc);

      if (vendor && product) {
        DefoldPlatform.writeGUID16(guid, 4, vendor);
        DefoldPlatform.writeGUID16(guid, 8, product);
        DefoldPlatform.writeGUID16(guid, 12, 0);
      } else {
        for (var i = 0; i < Math.min(name.length, 11); ++i) {
          guid[4 + i] = name.charCodeAt(i) & 0xff;
        }
      }

      if (is_xinput) {
        guid[14] = "x".charCodeAt(0);
      }

      return DefoldPlatform.formatGUID(guid);
    },

    getJoystickVendor: function(raw_gamepad_id) {
      var vendor_str = "Vendor: ";
      var vendor_str_index = raw_gamepad_id.indexOf(vendor_str);
      if (vendor_str_index >= 0) {
        return parseInt(raw_gamepad_id.substr(vendor_str_index + vendor_str.length, 4), 16) || 0;
      }

      var id_split = raw_gamepad_id.split("-");
      if (id_split.length > 1 && !isNaN(parseInt(id_split[0], 16))) {
        return parseInt(id_split[0], 16) || 0;
      }

      return 0;
    },

    getJoystickProduct: function(raw_gamepad_id) {
      var product_str = "Product: ";
      var product_str_index = raw_gamepad_id.indexOf(product_str);
      if (product_str_index >= 0) {
        return parseInt(raw_gamepad_id.substr(product_str_index + product_str.length, 4), 16) || 0;
      }

      var id_split = raw_gamepad_id.split("-");
      if (id_split.length > 1 && !isNaN(parseInt(id_split[1], 16))) {
        return parseInt(id_split[1], 16) || 0;
      }

      return 0;
    },

    disconnectJoystick: function (joy) {
      if (DefoldPlatform.gamepadFunc) {
        _free(DefoldPlatform.joys[joy].id);
        _free(DefoldPlatform.joys[joy].guid);
        delete DefoldPlatform.joys[joy];
        {{{ makeDynCall('vii', 'DefoldPlatform.gamepadFunc') }}}(joy, 0);
      }
    },

    joys: {}, // dmNative joystick data
    lastGamepadState: null,
    lastGamepadStateFrame: null, // The integer value of MainLoop.currentFrameNumber of when the last gamepad state was produced.

    refreshJoysticks: function(forceUpdate) {
        // Produce a new Gamepad API sample if we are ticking a new game frame, or if not using emscripten_set_main_loop() at all to drive animation.
        if (DefoldPlatform.gamepadFunc) {
          if (forceUpdate || MainLoop.currentFrameNumber !== DefoldPlatform.lastGamepadStateFrame || !MainLoop.currentFrameNumber) {
            DefoldPlatform.lastGamepadState = navigator.getGamepads ? navigator.getGamepads() : (navigator.webkitGetGamepads ? navigator.webkitGetGamepads : null);
            if (!DefoldPlatform.lastGamepadState) {
              return;
            }
            DefoldPlatform.lastGamepadStateFrame = MainLoop.currentFrameNumber;
            for (var joy = 0; joy < DefoldPlatform.lastGamepadState.length; ++joy) {
              var gamepad = DefoldPlatform.lastGamepadState[joy];

              if (gamepad) {
                var raw_gamepad_id = gamepad.id || "";
                var gamepad_id = (gamepad.mapping == "standard") ? "Standard Gamepad" : raw_gamepad_id;
                if (!DefoldPlatform.joys[joy] || DefoldPlatform.joys[joy].id_string != gamepad_id || DefoldPlatform.joys[joy].raw_id_string != raw_gamepad_id) {
                  if (DefoldPlatform.joys[joy]) {
                    //In case when user change gamepad while browser in background (minimized)
                    DefoldPlatform.disconnectJoystick(joy);
                  }
                  var vendor = DefoldPlatform.getJoystickVendor(raw_gamepad_id);
                  var product = DefoldPlatform.getJoystickProduct(raw_gamepad_id);
                  var is_xinput = raw_gamepad_id.toLowerCase().indexOf("xinput") >= 0;
                  if (!vendor && !product && is_xinput) {
                    vendor = 0x045e;
                    product = 0x028e;
                  }
                  var gamepad_guid = DefoldPlatform.createJoystickGUID(vendor, product, gamepad_id, is_xinput);
                  DefoldPlatform.joys[joy] = {
                    id: stringToNewUTF8(gamepad_id),
                    guid: stringToNewUTF8(gamepad_guid),
                    id_string: gamepad_id,
                    raw_id_string: raw_gamepad_id,
                    axesCount: gamepad.axes.length,
                    buttonsCount: gamepad.buttons.length
                  };
                  {{{ makeDynCall('vii', 'DefoldPlatform.gamepadFunc') }}}(joy, 1);
                }
                DefoldPlatform.joys[joy].buttons = gamepad.buttons;
                DefoldPlatform.joys[joy].axes = gamepad.axes;
              } else {
                if (DefoldPlatform.joys[joy]) {
                  DefoldPlatform.disconnectJoystick(joy);
                }
            }
          }
        }
      }
    }
  },

/*******************************************************************************
 * Native FUNCTIONS
 ******************************************************************************/

  /* Native initialization, termination and version querying */
  dmNativeInitJS: function() {


    DefoldPlatform.addEventListener("pagehide", DefoldPlatform.onWindowClose, true);
    DefoldPlatform.addEventListener("gamepadconnected", DefoldPlatform.onJoystickConnected, true);
    DefoldPlatform.addEventListener("gamepaddisconnected", DefoldPlatform.onJoystickDisconnected, true);
    DefoldPlatform.addEventListener("keydown", DefoldPlatform.onKeydown, true);
    DefoldPlatform.addEventListener("keypress", DefoldPlatform.onKeyPress, true);
    DefoldPlatform.addEventListener("keyup", DefoldPlatform.onKeyup, true);
    DefoldPlatform.addEventListener("mousemove", DefoldPlatform.onMousemove, true);
    DefoldPlatform.addEventListener("mousedown", DefoldPlatform.onMouseButtonDown, true);
    DefoldPlatform.addEventListener("mouseup", DefoldPlatform.onMouseButtonUp, true);
    DefoldPlatform.addEventListener('DOMMouseScroll', DefoldPlatform.onMouseWheel, { capture: true, passive: false });
    DefoldPlatform.addEventListener('mousewheel', DefoldPlatform.onMouseWheel, { capture: true, passive: false });
    DefoldPlatform.addEventListenerCanvas('touchstart', DefoldPlatform.onTouchStart, true);
    DefoldPlatform.addEventListenerCanvas('touchend', DefoldPlatform.onTouchEnd, true);
    DefoldPlatform.addEventListenerCanvas('touchcancel', DefoldPlatform.onTouchCancel, true);
    DefoldPlatform.addEventListenerCanvas('touchmove', DefoldPlatform.onTouchMove, true);
    DefoldPlatform.addEventListenerCanvas('focus', DefoldPlatform.onFocus, true);
    DefoldPlatform.addEventListenerCanvas('blur', DefoldPlatform.onBlur, true);

    // The browser can still be in fullscreen or pointer lock from a previous
    // engine instance, since a reboot keeps the canvas and dmNativeTerminate leaves
    // the browser state alone. Recompute the cached state from the document and
    // listen for changes again while it is active.
    DefoldPlatform.isFullscreen = false;
    DefoldPlatform.isPointerLocked = false;
    if (typeof document !== 'undefined') {
        DefoldPlatform.isFullscreen = DefoldPlatform.isDocumentFullscreen();
        if (DefoldPlatform.isFullscreen) {
            DefoldPlatform.addFullScreenEventListeners();
        }
        DefoldPlatform.isPointerLocked = !!document["pointerLockElement"];
        if (DefoldPlatform.isPointerLocked) {
            document.addEventListener('pointerlockchange', DefoldPlatform.onPointerLockEventChange, true);
        }
    }

    //TODO: Init with correct values
    DefoldPlatform.params = new Array();
    DefoldPlatform.cursorVisible = true; // NATIVE_MOUSE_CURSOR
    DefoldPlatform.stickyKeys = false; // NATIVE_STICKY_KEYS
    DefoldPlatform.stickyMouseButtons = true; // NATIVE_STICKY_MOUSE_BUTTONS
    DefoldPlatform.systemKeys = false; // NATIVE_SYSTEM_KEYS
    DefoldPlatform.keyRepeat = false; // NATIVE_KEY_REPEAT
    DefoldPlatform.autoPollEvents = true; // NATIVE_AUTO_POLL_EVENTS
    DefoldPlatform.params[1] = true; // NATIVE_OPENED
    DefoldPlatform.params[2] = true; // NATIVE_ACTIVE
    DefoldPlatform.params[3] = false; // NATIVE_ICONIFIED
    DefoldPlatform.params[4] = true; // NATIVE_ACCELERATED
    DefoldPlatform.params[5] = 0; // NATIVE_RED_BITS
    DefoldPlatform.params[6] = 0; // NATIVE_GREEN_BITS
    DefoldPlatform.params[7] = 0; // NATIVE_BLUE_BITS
    DefoldPlatform.params[8] = 0; // NATIVE_ALPHA_BITS
    DefoldPlatform.params[9] = 0; // NATIVE_DEPTH_BITS
    DefoldPlatform.params[10] = 0; // NATIVE_STENCIL_BITS
    DefoldPlatform.params[11] = 0; // NATIVE_REFRESH_RATE
    DefoldPlatform.params[12] = 0; // NATIVE_ACCUM_RED_BITS
    DefoldPlatform.params[13] = 0; // NATIVE_ACCUM_GREEN_BITS
    DefoldPlatform.params[14] = 0; // NATIVE_ACCUM_BLUE_BITS
    DefoldPlatform.params[15] = 0; // NATIVE_ACCUM_ALPHA_BITS
    DefoldPlatform.params[16] = 0; // NATIVE_AUX_BUFFERS
    DefoldPlatform.params[17] = 0; // NATIVE_STEREO
    DefoldPlatform.params[18] = 0; // NATIVE_WINDOW_NO_RESIZE
    DefoldPlatform.params[19] = 0; // NATIVE_FSAA_SAMPLES
    DefoldPlatform.params[21] = 0; // NATIVE_WINDOW_HIGH_DPI

    DefoldPlatform.dpi = 1;

    DefoldPlatform.keys = new Array();

    DefoldPlatform.NATIVE_PHASE_BEGAN = 0;
    DefoldPlatform.NATIVE_PHASE_MOVED = 1;
    DefoldPlatform.NATIVE_PHASE_ENDED = 3;
    DefoldPlatform.NATIVE_PHASE_CANCELLED = 4;

    return 1; // GL_TRUE
  },

  dmNativeTerminate: () => {
    DefoldPlatform.removeEventListener("pagehide", DefoldPlatform.onWindowClose, true);
    DefoldPlatform.removeEventListener("gamepadconnected", DefoldPlatform.onJoystickConnected, true);
    DefoldPlatform.removeEventListener("gamepaddisconnected", DefoldPlatform.onJoystickDisconnected, true);
    DefoldPlatform.removeEventListener("keydown", DefoldPlatform.onKeydown, true);
    DefoldPlatform.removeEventListener("keypress", DefoldPlatform.onKeyPress, true);
    DefoldPlatform.removeEventListener("keyup", DefoldPlatform.onKeyup, true);
    DefoldPlatform.removeEventListener("mousemove", DefoldPlatform.onMousemove, true);
    DefoldPlatform.removeEventListener("mousedown", DefoldPlatform.onMouseButtonDown, true);
    DefoldPlatform.removeEventListener("mouseup", DefoldPlatform.onMouseButtonUp, true);
    DefoldPlatform.removeEventListener('DOMMouseScroll', DefoldPlatform.onMouseWheel, { capture: true, passive: false });
    DefoldPlatform.removeEventListener('mousewheel', DefoldPlatform.onMouseWheel, { capture: true, passive: false });
    DefoldPlatform.removeEventListenerCanvas('touchstart', DefoldPlatform.onTouchStart, true);
    DefoldPlatform.removeEventListenerCanvas('touchend', DefoldPlatform.onTouchEnd, true);
    DefoldPlatform.removeEventListenerCanvas('touchcancel', DefoldPlatform.onTouchCancel, true);
    DefoldPlatform.removeEventListenerCanvas('touchmove', DefoldPlatform.onTouchMove, true);
    DefoldPlatform.removeEventListenerCanvas('focus', DefoldPlatform.onFocus, true);
    DefoldPlatform.removeEventListenerCanvas('blur', DefoldPlatform.onBlur, true);

    // Fullscreen and pointer lock listeners are otherwise only removed by their
    // own change handlers, so they survive termination if we exit while active.
    // The browser state itself is left as it is, dmNativeInitJS recomputes the
    // cached flags from the document when a new window is opened.
    // The document is guarded like window/canvas above, since dmNativeTerminate is
    // also reachable from embeddings without a DOM.
    if (typeof document !== 'undefined') {
        DefoldPlatform.removeFullScreenEventListeners();
        document.removeEventListener('pointerlockchange', DefoldPlatform.onPointerLockEventChange, true);
    }

    // The callbacks point into the engine that is being destroyed. They are set
    // again when a new window is opened.
    DefoldPlatform.keyFunc = null;
    DefoldPlatform.charFunc = null;
    DefoldPlatform.markedTextFunc = null;
    DefoldPlatform.gamepadFunc = null;
    DefoldPlatform.mouseButtonFunc = null;
    DefoldPlatform.mousePosFunc = null;
    DefoldPlatform.mouseWheelFunc = null;
    DefoldPlatform.resizeFunc = null;
    DefoldPlatform.closeFunc = null;
    DefoldPlatform.refreshFunc = null;
    DefoldPlatform.focusFunc = null;
    DefoldPlatform.iconifyFunc = null;
    DefoldPlatform.touchFunc = null;
  },

  dmNativeOpenWindowJS__deps: ['$Browser'],
  dmNativeOpenWindowJS: function(width, height, alphabits, samples, fullscreen, highDPI, useWebGL, webglVersion) {
    if (width == 0 && height > 0) {
      width = 4 * height / 3;
    }
    if (width > 0 && height == 0) {
      height = 3 * width / 4;
    }
    DefoldPlatform.params[5] = 8; // NATIVE_RED_BITS
    DefoldPlatform.params[6] = 8; // NATIVE_GREEN_BITS
    DefoldPlatform.params[7] = 8; // NATIVE_BLUE_BITS
    DefoldPlatform.params[8] = alphabits; // NATIVE_ALPHA_BITS
    DefoldPlatform.params[9] = 32; // NATIVE_DEPTH_BITS
    DefoldPlatform.params[10] = 8; // NATIVE_STENCIL_BITS

    if (!fullscreen) {
      DefoldPlatform.initWindowWidth = width;
      DefoldPlatform.initWindowHeight = height;
      DefoldPlatform.stickyMouseButtons = true; // NATIVE_STICKY_MOUSE_BUTTONS
    } else {
      DefoldPlatform.requestFullScreen();
      DefoldPlatform.stickyMouseButtons = false; // NATIVE_STICKY_MOUSE_BUTTONS
    }

    DefoldPlatform.params[19] = samples;
    DefoldPlatform.params[21] = highDPI;
    DefoldPlatform.dpi = highDPI ? (window.devicePixelRatio || 1) : 1;
    if(useWebGL) {
        var contextAttributes = {
            antialias: (DefoldPlatform.params[19] > 1), // NATIVE_FSAA_SAMPLES
            depth: (DefoldPlatform.params[9] > 0), // NATIVE_DEPTH_BITS
            stencil: (DefoldPlatform.params[10] > 0), // NATIVE_STENCIL_BITS
            alpha: (DefoldPlatform.params[8] > 0), // NATIVE_ALPHA_BITS
            majorVersion: webglVersion
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
    }
    return 1; // GL_TRUE
  },

  dmNativeCloseWindow: function() {
    if (DefoldPlatform.closeFunc) {
      {{{ makeDynCall('i', 'DefoldPlatform.closeFunc') }}}();
    }
    delete Module.ctx;
  },

  dmNativeSetWindowTitle: function(title) {
    document.title = UTF8ToString(title);
  },

  dmNativeGetWindowSize: function(width, height) {
    setValue(width, Module['canvas'].width, 'i32');
    setValue(height, Module['canvas'].height, 'i32');
  },

  dmNativeSetWindowSize: function(width, height) {
      Browser.setCanvasSize(width, height);
      if (DefoldPlatform.resizeFunc) {
        {{{ makeDynCall('vii', 'DefoldPlatform.resizeFunc') }}}(width, height);
      }
  },

  dmNativeSetWindowPos: function(x, y) {},

  dmNativeIconifyWindow: function() {},

  dmNativeSwapBuffers__deps: ['dmNativeSetWindowSize'],
  dmNativeSwapBuffers: function() {

    var width = Module['canvas'].width;
    var height = Module['canvas'].height;

    if (DefoldPlatform.prevWidth != width || DefoldPlatform.prevHeight != height) {
      if (DefoldPlatform.isFullscreen) {
        width = Math.floor(window.innerWidth * DefoldPlatform.dpi);
        height = Math.floor(window.innerHeight * DefoldPlatform.dpi);
      }
      DefoldPlatform.prevWidth = width;
      DefoldPlatform.prevHeight = height;
      _dmNativeSetWindowSize(width, height);
    }
  },

  dmNativeSwapInterval: function(interval) {},

  dmNativeGetWindowParam: function(param) {
    return DefoldPlatform.params[param];
  },

  dmNativeSetWindowSizeCallback: function(cbfun) {
    DefoldPlatform.resizeFunc = cbfun;
  },

  dmNativeSetWindowCloseCallback: function(cbfun) {
    DefoldPlatform.closeFunc = cbfun;
  },

  dmNativeSetWindowFocusCallback: function(cbfun) {
    DefoldPlatform.focusFunc = cbfun;
  },

  dmNativeSetWindowIconifyCallback: function(cbfun) {
    DefoldPlatform.iconifyFunc = cbfun;
  },

  /* Video mode functions */
  dmNativePollEvents: function() {},

  dmNativeGetKey: function(key) {
    return DefoldPlatform.keys[key];
  },

  dmNativeGetMouseButton: function(button) {
    return (DefoldPlatform.buttons & (1 << DefoldPlatform.DOMtoNativeButton(button))) > 0;
  },

  dmNativeGetMousePos: function(xpos, ypos) {
    setValue(xpos, Browser.mouseX, 'i32');
    setValue(ypos, Browser.mouseY, 'i32');
  },

  // I believe it is not possible to move the mouse with javascript
  dmNativeGetMouseWheel: function() {
    return DefoldPlatform.wheelPos;
  },

  dmNativeSetCursorVisible: function(visible) {
    DefoldPlatform.cursorVisible = !!visible;
    if (visible) DefoldPlatform.cancelPointerLock();
    else DefoldPlatform.requestPointerLock();
  },

  dmNativeGetMouseLocked: function() {
    return DefoldPlatform.isPointerLocked ? 1 : 0;
  },

  dmNativeSetCharCallback: function(cbfun) {
    DefoldPlatform.charFunc = cbfun;
    return 1;
  },

  dmNativeSetMarkedTextCallback: function(cbfun) {
    DefoldPlatform.markedTextFunc = cbfun;
    return 1;
  },

  dmNativeSetGamepadCallback: function(cbfun) {
    DefoldPlatform.gamepadFunc = cbfun;
    try {
      DefoldPlatform.refreshJoysticks();
      return 1;
    }
    catch(e) {
      console.error(e);
      DefoldPlatform.gamepadFunc = null;
      return 0;
    }
  },

  dmNativeSetDeviceChangedCallback: function(cbfun) {
    return 1;
  },

  /* Joystick input */

  dmNativeGetJoystickParam: function(joy, param) {
    var result = 0; //GL_FALSE
    if (DefoldPlatform.joys[joy]) {
      switch (param) {
        case 0x00050001: // NATIVE_PRESENT
          result = 1; //GL_TRUE
          break;
        case 0x00050002: // NATIVE_AXES
          result = DefoldPlatform.joys[joy].axesCount;
          break;
        case 0x00050003: // NATIVE_BUTTONS
          result = DefoldPlatform.joys[joy].buttonsCount;
          break;
        }
    }
    return result;
  },

  dmNativeGetJoystickPos: function(joy, pos, numaxes) {
    DefoldPlatform.refreshJoysticks();
    var state = DefoldPlatform.joys[joy];
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

  dmNativeGetJoystickButtons: function(joy, buttons, numbuttons) {
    DefoldPlatform.refreshJoysticks();
    var state = DefoldPlatform.joys[joy];
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

  dmNativeGetJoystickHats: function(joy, buttons, numhats) {
    return 0;
  },

  dmNativeGetJoystickDeviceId: function(joy, device_id) {
    if (DefoldPlatform.joys[joy]) {
      setValue(device_id, DefoldPlatform.joys[joy].id, '*');
      return 1;
    } else {
      return 0;
    }
  },

  dmNativeGetJoystickDeviceGuid: function(joy, device_guid) {
    if (DefoldPlatform.joys[joy]) {
      setValue(device_guid, DefoldPlatform.joys[joy].guid, '*');
      return 1;
    } else {
      return 0;
    }
  },

  dmNativeCreateJoystickDeviceGuid: function(bus, vendor, product, version, vendor_name, product_name, driver_signature, driver_data, guid) {
    function crc16ForByte(value) {
      var crc = 0;
      for (var bit = 0; bit < 8; ++bit) {
        crc = ((((crc ^ value) & 1) ? 0xA001 : 0) ^ (crc >> 1)) & 0xffff;
        value >>= 1;
      }
      return crc;
    }
    function crc16(crc, string) {
      if (string) {
        for (var i = 0; i < string.length; ++i) {
          crc = (crc16ForByte((crc & 0xff) ^ string.charCodeAt(i)) ^ (crc >> 8)) & 0xffff;
        }
      }
      return crc;
    }
    function setGuidWord(guidData, offset, value) {
      guidData[offset + 0] = value & 0xff;
      guidData[offset + 1] = (value >> 8) & 0xff;
    }

    var vendorName = vendor_name ? UTF8ToString(vendor_name) : null;
    var productName = product_name ? UTF8ToString(product_name) : null;
    var guidData = new Array(16).fill(0);
    var crc = 0;

    if (vendorName && vendorName.length && productName && productName.length) {
      crc = crc16(crc, vendorName);
      crc = crc16(crc, ' ');
      crc = crc16(crc, productName);
    } else if (productName) {
      crc = crc16(crc, productName);
    }

    setGuidWord(guidData, 0, bus);
    setGuidWord(guidData, 2, crc);

    if (vendor) {
      setGuidWord(guidData, 4, vendor);
      setGuidWord(guidData, 8, product);
      setGuidWord(guidData, 12, version);
      guidData[14] = driver_signature;
      guidData[15] = driver_data;
    } else {
      var availableSpace = guidData.length - 4;
      if (driver_signature) {
        availableSpace -= 2;
        guidData[14] = driver_signature;
        guidData[15] = driver_data;
      }
      if (productName) {
        for (var j = 0; j + 1 < availableSpace && j < productName.length; ++j) {
          guidData[4 + j] = productName.charCodeAt(j);
        }
      }
    }

    var hex = '0123456789abcdef';
    for (var k = 0; k < guidData.length; ++k) {
      setValue(guid + k * 2 + 0, hex.charCodeAt((guidData[k] >> 4) & 0x0f), 'i8');
      setValue(guid + k * 2 + 1, hex.charCodeAt(guidData[k] & 0x0f), 'i8');
    }
    setValue(guid + 32, 0, 'i8');
  },

  /* OpenGL extensions */
  dmNativeGetProcAddress__deps: ['emscripten_GetProcAddress'],
  dmNativeGetProcAddress: function(procname) {
    return _emscripten_GetProcAddress(procname);
  },

  dmNativeShowKeyboard: function(show_keyboard) {
    Module['canvas'].contentEditable = show_keyboard ? true : false;
    if (show_keyboard) {
      Module['canvas'].focus();
    }
  },

  dmNativeResetKeyboard: function() {
  },

  dmNativeSetTouchCallback: function(cbfun) {
    DefoldPlatform.touchFunc = cbfun;
    return 1;
  },

  dmNativeGetAcceleration: function(x, y, z) {
      return 0;
  },

  dmNativeGetWindowRefreshRate: function() {
    return 0;
  },

  dmNativeGetDefaultFramebuffer: function() {
	  return 0;
  },

  dmNativeAccelerometerEnable: function() {
  },

  dmNativeSetWindowBackgroundColor: function(color) {
  },

  dmNativeGetDisplayScaleFactor: function() {
    return DefoldPlatform.dpi;
  }
};

autoAddDeps(LibraryDefoldPlatform, '$DefoldPlatform');
addToLibrary(LibraryDefoldPlatform);
