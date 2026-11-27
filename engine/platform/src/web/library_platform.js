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

var LibraryDefoldPlatform = {
  $DefoldPlatform__deps: ['$Browser', '$MainLoop'],
  $DefoldPlatform: {

    callbacks: {},
    listeners: [],
    initialized: false,
    watchingDisplay: false,
    fullscreenTarget: null,
    contextCanvas: null,
    contextUsesWebGL: false,
    isFullscreen: false,
    isPointerLocked: false,
    NATIVE_PHASE_BEGAN: 0,
    NATIVE_PHASE_MOVED: 1,
    NATIVE_PHASE_ENDED: 3,
    NATIVE_PHASE_CANCELLED: 4,

    // Named keys follow the mobile/browser key values in window_mobile_web_constants.h. Printable letters
    // use the layout-dependent DOM value, while punctuation uses the physical key.
    namedKeys: {
      Escape: 257, ArrowUp: 283, ArrowDown: 284, ArrowLeft: 285, ArrowRight: 286,
      ShiftLeft: 287, ShiftRight: 288, ControlLeft: 289, ControlRight: 290, AltLeft: 291, AltRight: 292,
      Tab: 293, Enter: 294, Backspace: 295, Insert: 296, Delete: 297,
      PageUp: 298, PageDown: 299, Home: 300, End: 301,
      NumpadDivide: 312, NumpadMultiply: 313, NumpadSubtract: 314, NumpadAdd: 315,
      NumpadDecimal: 316, NumpadComma: 316, NumpadEqual: 317, NumpadEnter: 318,
      NumLock: 319, CapsLock: 320, ScrollLock: 321, Pause: 322,
      MetaLeft: 323, MetaRight: 324, ContextMenu: 325,
      Space: 32, Quote: 39, Comma: 44, Minus: 45, Period: 46, Slash: 47, Semicolon: 59,
      Equal: 61, BracketLeft: 91, Backslash: 92, IntlRo: 92, IntlYen: 92, IntlBackslash: 92,
      BracketRight: 93, Backquote: 96
    },

    // Older browsers and virtual keyboards can omit KeyboardEvent.code.
    legacyKeyNames: {
      1: 'AltRight', 2: 'AltLeft', 3: 'ControlLeft', 4: 'ControlRight', 5: 'ShiftLeft', 6: 'ShiftRight',
      8: 'Backspace', 9: 'Tab', 13: 'Enter',
      27: 'Escape', 33: 'PageUp', 34: 'PageDown', 35: 'End', 36: 'Home',
      37: 'ArrowLeft', 38: 'ArrowUp', 39: 'ArrowRight', 40: 'ArrowDown', 45: 'Insert', 46: 'Delete',
      106: 'NumpadMultiply', 107: 'NumpadAdd', 109: 'NumpadSubtract', 110: 'NumpadDecimal', 111: 'NumpadDivide'
    },

    keyNumber: function(event) {
      var value = event.keyCode;
      var code = event.code;
      var modifierCode = /^(Shift|Control|Alt)(Left|Right)$/.exec(code);
      var modifier = event.key === 'AltGraph' ? 'Alt' : event.key;

      // Use the reported function and side to preserve OS modifier remaps.
      if (modifier !== 'Shift' && modifier !== 'Control' && modifier !== 'Alt') {
        switch (value) {
          case 16: modifier = 'Shift'; break;
          case 17: modifier = 'Control'; break;
          case 18:
          case 225: modifier = 'Alt'; break;
          default: modifier = !value && modifierCode ? modifierCode[1] : null; break;
        }
      }
      if (modifier) {
        // XKB can report AltGraph from MetaRight with a standard location.
        var right = event.location === 2 || (event.location !== 1 &&
          ((modifierCode && modifierCode[2] === 'Right') || code === 'MetaRight'));
        return DefoldPlatform.namedKeys[modifier + (right ? 'Right' : 'Left')];
      }

      if (value >= 112 && value <= 123) return 258 + value - 112;
      if (value >= 96 && value <= 105) return 302 + value - 96;
      if (code === 'NumpadEnter' && value === 13) return DefoldPlatform.namedKeys.NumpadEnter;
      var named = DefoldPlatform.namedKeys[DefoldPlatform.legacyKeyNames[value]] ||
        (!modifierCode && DefoldPlatform.namedKeys[code]);
      if (named) return named;
      if (value) return value;
      if (/^F([1-9]|1[0-2])$/.test(code)) return 257 + Number(code.slice(1));
      if (/^Numpad[0-9]$/.test(code)) return 302 + Number(code[6]);
      return event.key && event.key.length === 1 ? event.key.toUpperCase().charCodeAt(0) : 0;
    },

    isCanvasActive: function(event) {
      var target = event.target;
      var canvas = Module['canvas'];
      var active = document.activeElement === undefined || document.activeElement === canvas || target === canvas;
      // Allow page controls to take focus even when the game handles the event.
      if (target && target.focus && document.activeElement !== target) target.focus();
      return active;
    },

    onKeydown: function(event) {
      var platform = DefoldPlatform;
      // The first and last IME keydowns can report 229 outside isComposing.
      if (event.isComposing || event.keyCode === 229 || !platform.isCanvasActive(event)) return;
      var key = platform.keyNumber(event);
      if (key) platform.keys[key] = 1;
      if (key === 32 || (key >= 283 && key <= 286)) {
        event.preventDefault();
        event.stopPropagation();
        // Preventing the default for Space also suppresses its keypress.
        if (key === 32 && platform.callbacks.character) {
          {{{ makeDynCall('vii', 'DefoldPlatform.callbacks.character') }}}(32, 1);
        }
      } else if (key === 293 || key === 294 || key === 295 || key === 318) {
        event.preventDefault();
      }
    },

    onKeyup: function(event) {
      if (DefoldPlatform.isCanvasActive(event)) {
        DefoldPlatform.keys[DefoldPlatform.keyNumber(event)] = 0;
      }
    },

    onKeyPress: function(event) {
      if (!event.isComposing && event.charCode && DefoldPlatform.callbacks.character && DefoldPlatform.isCanvasActive(event)) {
        {{{ makeDynCall('vii', 'DefoldPlatform.callbacks.character') }}}(event.charCode, 1);
      }
    },

    onMousemove: function(event) {
      Browser.calculateMouseEvent(event);
      DefoldPlatform.mousePositionFromTouch = false;
    },

    fillTouch: function(id, x, y, phase) {
      if (DefoldPlatform.callbacks.touch) {
        {{{ makeDynCall('viiii', 'DefoldPlatform.callbacks.touch') }}}(id, x, y, phase);
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
              DefoldPlatform.mouseLeftButtonFromTouch = false;
            }
        }

        if (event.touches.length == 0){
            DefoldPlatform.buttons &= ~(1 << 0);
            DefoldPlatform.mouseLeftButtonFromTouch = false;
        }

        DefoldPlatform.resumeAudio();

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
            DefoldPlatform.mousePositionFromTouch = true;
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
            DefoldPlatform.mouseLeftButtonFromTouch = true;
            Browser.mouseX = canvasX;
            Browser.mouseY = canvasY;
            DefoldPlatform.mousePositionFromTouch = true;
          }
          DefoldPlatform.fillTouch(touch.identifier, canvasX, canvasY, DefoldPlatform.NATIVE_PHASE_BEGAN);
        }

        DefoldPlatform.resumeAudio();

        event.preventDefault();
    },

    // DOM button order is left, middle, right. Defold uses left, right, middle.
    buttonMasks: [1, 4, 2, 8, 16, 32, 64, 128],

    onMouseButtonDown: function(event) {
      if (event.target !== Module['canvas']) return;
      DefoldPlatform.isCanvasActive(event);
      DefoldPlatform.onMousemove(event);
      DefoldPlatform.buttons |= (DefoldPlatform.buttonMasks[event.button] || 0);
      if (event.button === 0) DefoldPlatform.mouseLeftButtonFromTouch = false;
      DefoldPlatform.resumeAudio();
    },

    onMouseButtonUp: function(event) {
      if (!DefoldPlatform.isCanvasActive(event)) return;
      DefoldPlatform.onMousemove(event);
      DefoldPlatform.buttons &= ~(DefoldPlatform.buttonMasks[event.button] || 0);
      if (event.button === 0) DefoldPlatform.mouseLeftButtonFromTouch = false;
      DefoldPlatform.resumeAudio();
    },

    resumeAudio: function() {
      if (typeof DefoldSoundDevice !== 'undefined' && DefoldSoundDevice) DefoldSoundDevice.TryResumeAudio();
    },

    onMouseWheel: function(event) {
      if (DefoldPlatform.isCanvasActive(event)) {
        // Emscripten preserves the DOM sign. Only legacy wheelDelta is positive for scrolling up.
        var delta = Browser.getMouseWheelDelta(event);
        DefoldPlatform.wheelPos += event.type === 'mousewheel' ? delta : -delta;
        if (event.cancelable && event.target === Module['canvas']) event.preventDefault();
      }
    },

    onFocusChanged: function(focused) {
      DefoldPlatform.params[2] = focused;
      if (!focused) {
        DefoldPlatform.keys.fill(0);
        DefoldPlatform.buttons = 0;
        DefoldPlatform.mouseTouchId = null;
        DefoldPlatform.mouseLeftButtonFromTouch = false;
      }
      if (DefoldPlatform.callbacks.focus) {
        {{{ makeDynCall('vi', 'DefoldPlatform.callbacks.focus') }}}(focused);
      }
    },

    isGameFullscreen: function(canvas) {
      var element = document.fullscreenElement || document.webkitFullscreenElement ||
        document.mozFullScreenElement || document.msFullscreenElement;
      if (element) return element === canvas || element === Module['fullScreenContainer'] ||
        element === DefoldPlatform.fullscreenTarget || !!(canvas && element.contains && element.contains(canvas));
      return !!(DefoldPlatform.fullscreenTarget && (document.fullScreen || document.mozFullScreen ||
        document.webkitIsFullScreen || document.msIsFullScreen));
    },

    readDisplayState: function() {
      var canvas = Module['canvas'];
      DefoldPlatform.isFullscreen = DefoldPlatform.isGameFullscreen(canvas);
      DefoldPlatform.isPointerLocked = !!canvas && document.pointerLockElement === canvas;
    },

    watchDisplay: function() {
      var platform = DefoldPlatform;
      if (platform.watchingDisplay || typeof document === 'undefined') return;
      platform.listen(document, ['fullscreenchange', 'webkitfullscreenchange', 'mozfullscreenchange',
        'MSFullscreenChange', 'msfullscreenchange', 'fullscreenerror', 'webkitfullscreenerror',
        'mozfullscreenerror', 'MSFullscreenError'], function() {
        platform.readDisplayState();
        if (!platform.isFullscreen) platform.fullscreenTarget = null;
      }, true);
      platform.listen(document, ['pointerlockchange'], platform.readDisplayState, true);
      platform.watchingDisplay = true;
      platform.readDisplayState();
    },

    requestFullScreen: function(element) {
      var target = element || Module['fullScreenContainer'] || Module['canvas'];
      if (!target) return;
      // The HTML loader can request fullscreen before the engine opens its first window.
      DefoldPlatform.watchDisplay();
      var request = target.requestFullscreen || target.webkitRequestFullscreen || target.webkitRequestFullScreen ||
        target.mozRequestFullScreen || target.msRequestFullscreen || target.msRequestFullScreen || target.requestFullScreen;
      if (request) {
        DefoldPlatform.fullscreenTarget = target;
        request.call(target);
      }
    },

    cancelFullScreen: function() {
      var exit = document.exitFullscreen || document.webkitExitFullscreen || document.webkitCancelFullScreen ||
        document.mozCancelFullScreen || document.msExitFullscreen || document.cancelFullScreen;
      if (exit) exit.call(document);
    },

    listen: function(target, events, listener, options) {
      if (!target || !target.addEventListener) return;
      for (var i = 0; i < events.length; ++i) {
        var event = events[i];
        target.addEventListener(event, listener, options);
        DefoldPlatform.listeners.push([target, event, listener, options]);
      }
    },

    listenToCanvas: function(canvas) {
      var platform = DefoldPlatform;
      platform.listen(canvas, ['touchstart'], platform.onTouchStart, true);
      platform.listen(canvas, ['touchmove'], platform.onTouchMove, true);
      platform.listen(canvas, ['touchend'], platform.onTouchEnd, true);
      platform.listen(canvas, ['touchcancel'], platform.onTouchCancel, true);
      platform.listen(canvas, ['focus'], function() { platform.onFocusChanged(1); }, true);
      platform.listen(canvas, ['blur'], function() { platform.onFocusChanged(0); }, true);
    },

    prepareCanvas: function(useWebGL) {
      var platform = DefoldPlatform;
      var previous = Module['canvas'];
      if (previous && previous === platform.contextCanvas && useWebGL !== platform.contextUsesWebGL) {
        // WebGPU and WebGL need separate canvases. Preserve the DOM attributes and rebind the listeners.
        var canvas = previous.cloneNode(true);
        var focused = document.activeElement === previous;
        platform.listeners = platform.listeners.filter(function(entry) {
          if (entry[0] !== previous) return true;
          previous.removeEventListener(entry[1], entry[2], entry[3]);
          return false;
        });
        if (Module['setCanvasEventListeners']) Module['setCanvasEventListeners'](previous, false);
        if (previous.parentNode) previous.parentNode.replaceChild(canvas, previous);
        Module['canvas'] = canvas;
        if (Module['fullScreenContainer'] === previous) Module['fullScreenContainer'] = canvas;
        if (platform.fullscreenTarget === previous) platform.fullscreenTarget = null;
        if (focused) canvas.focus();
        platform.listenToCanvas(canvas);
        if (Module['setCanvasEventListeners']) Module['setCanvasEventListeners'](canvas, true);
        platform.readDisplayState();
      }
      platform.contextCanvas = Module['canvas'];
      platform.contextUsesWebGL = useWebGL;
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

    joys: {},
    gamepadFrame: -1,

    forgetJoystick: function(slot) {
      var pad = DefoldPlatform.joys[slot];
      delete DefoldPlatform.joys[slot];
      _free(pad.id);
      _free(pad.guid);
    },

    refreshJoysticks: function(force) {
      var platform = DefoldPlatform;
      var frame = MainLoop.currentFrameNumber;
      if (!platform.callbacks.gamepad || (!force && frame && frame === platform.gamepadFrame)) return;
      var getPads = navigator.getGamepads || navigator.webkitGetGamepads;
      if (!getPads) return;
      var pads = getPads.call(navigator);
      if (!pads) return;
      platform.gamepadFrame = frame;

      // Browsers may shorten the array or replace a device in place while backgrounded.
      Object.keys(platform.joys).forEach(function(slot) {
        var pad = pads[slot];
        var old = platform.joys[slot];
        if (!pad || old.rawId !== (pad.id || '') || old.mapping !== pad.mapping) {
          platform.forgetJoystick(slot);
          {{{ makeDynCall('vii', 'DefoldPlatform.callbacks.gamepad') }}}(Number(slot), 0);
        }
      });
      for (var slot = 0; slot < pads.length; ++slot) {
        var pad = pads[slot];
        if (!pad) continue;
        var known = platform.joys[slot];
        if (known) {
          known.sample = pad;
          continue;
        }
        var raw = pad.id || '';
        var name = pad.mapping === 'standard' ? 'Standard Gamepad' : raw;
        var vendor = platform.getJoystickVendor(raw);
        var product = platform.getJoystickProduct(raw);
        var xinput = /xinput/i.test(raw);
        if (xinput && !vendor && !product) {
          vendor = 0x045e;
          product = 0x028e;
        }
        var guid = platform.createJoystickGUID(vendor, product, name, xinput);
        platform.joys[slot] = { rawId: raw, mapping: pad.mapping, sample: pad,
          id: stringToNewUTF8(name), guid: stringToNewUTF8(guid) };
        {{{ makeDynCall('vii', 'DefoldPlatform.callbacks.gamepad') }}}(slot, 1);
      }
    },

    onGamepadChange: function() { DefoldPlatform.refreshJoysticks(true); },

    resetInput: function() {
      var platform = DefoldPlatform;
      platform.keys.fill(0);
      platform.buttons = platform.wheelPos = 0;
      platform.mouseTouchId = null;
      platform.mouseLeftButtonFromTouch = platform.mousePositionFromTouch = false;
    }
  },

  dmNativeInitJS: function() {
    var platform = DefoldPlatform;
    if (platform.initialized) return 1;
    platform.initialized = true;
    platform.keys = new Uint8Array(327);
    platform.params = new Int32Array(23); // WindowState in dmsdk/platform/window.h
    platform.params[2] = platform.params[4] = 1;
    platform.prevWidth = platform.prevHeight = 0;
    platform.resetInput();
    if (typeof window !== 'undefined') {
      platform.listen(window, ['pagehide'], function() { platform.params[1] = 0; }, true);
      platform.listen(window, ['gamepadconnected', 'gamepaddisconnected'], platform.onGamepadChange, true);
      platform.listen(window, ['keydown'], platform.onKeydown, true);
      platform.listen(window, ['keyup'], platform.onKeyup, true);
      platform.listen(window, ['keypress'], platform.onKeyPress, true);
      platform.listen(window, ['mousemove'], platform.onMousemove, true);
      platform.listen(window, ['mousedown'], platform.onMouseButtonDown, true);
      platform.listen(window, ['mouseup'], platform.onMouseButtonUp, true);
      var wheelEvents = 'onwheel' in window ? ['wheel'] : ['DOMMouseScroll', 'mousewheel'];
      platform.listen(window, wheelEvents, platform.onMouseWheel, { capture: true, passive: false });
    }
    platform.listenToCanvas(Module['canvas']);
    platform.watchDisplay();
    return 1;
  },

  dmNativeTerminate: function() {
    var platform = DefoldPlatform;
    platform.listeners.forEach(function(entry) { entry[0].removeEventListener(entry[1], entry[2], entry[3]); });
    platform.listeners.length = 0;
    platform.watchingDisplay = false;
    Object.keys(platform.joys).forEach(platform.forgetJoystick);
    platform.gamepadFrame = -1;
    platform.callbacks = {};
    platform.initialized = false;
    if (platform.params) platform.params[1] = 0;
  },

  dmNativeOpenWindowJS: function(alphaBits, samples, fullscreen, highDPI, useWebGL, version) {
    var platform = DefoldPlatform;
    platform.prepareCanvas(!!useWebGL);
    if (fullscreen) platform.requestFullScreen();
    if (useWebGL) {
      // WebGL 2 contexts on iOS before 15.2 could be created but failed to render.
      var ios = /CPU(?: iPhone)? OS (\d+)[_.](\d+)/.exec(navigator.userAgent);
      var legacy = ios && (Number(ios[1]) < 15 || (Number(ios[1]) === 15 && Number(ios[2]) < 2));
      var attributes = { alpha: alphaBits > 0, antialias: samples > 1, depth: true,
        stencil: true, majorVersion: legacy ? 1 : version };
      var canvas = Module['canvas'];
      Module.ctx = Browser.createContext(canvas, true, true, attributes);
      if (!Module.ctx && attributes.majorVersion !== 1) {
        attributes.majorVersion = 1;
        Module.ctx = Browser.createContext(canvas, true, true, attributes);
      }
      if (!Module.ctx) return 0;
    }
    platform.params.set([8, 8, 8, alphaBits, 32, 8], 5);
    platform.params[19] = samples;
    platform.params[21] = highDPI;
    platform.params[1] = 1;
    platform.resetInput();
    return 1;
  },

  dmNativeCloseWindow: function() {
    // Graphics startup can close the window between adapter attempts.
    DefoldPlatform.params[1] = 0;
    delete Module.ctx;
  },

  dmNativeSetWindowTitle: function(title) { document.title = UTF8ToString(title); },

  dmNativeGetWindowSize: function(width, height) {
    var canvas = Module['canvas'];
    if (width) setValue(width, canvas.width, 'i32');
    if (height) setValue(height, canvas.height, 'i32');
  },

  dmNativeSetWindowSize: function(width, height) {
    Browser.setCanvasSize(width, height);
    DefoldPlatform.prevWidth = width;
    DefoldPlatform.prevHeight = height;
    if (DefoldPlatform.callbacks.resize) {
      {{{ makeDynCall('vii', 'DefoldPlatform.callbacks.resize') }}}(width, height);
    }
  },

  dmNativeSwapBuffers__deps: ['dmNativeSetWindowSize'],
  dmNativeSwapBuffers: function() {

    var width = Module['canvas'].width;
    var height = Module['canvas'].height;

    if (DefoldPlatform.isFullscreen) {
      var dpi = Module['getPixelRatio'](DefoldPlatform.params[21]); // WINDOW_STATE_HIGH_DPI
      width = Math.max(1, Math.floor(window.innerWidth * dpi));
      height = Math.max(1, Math.floor(window.innerHeight * dpi));
    }

    if (DefoldPlatform.prevWidth != width || DefoldPlatform.prevHeight != height ||
        Module['canvas'].width != width || Module['canvas'].height != height) {
      _dmNativeSetWindowSize(width, height);
    }
  },

  dmNativeGetWindowParam: function(param) { return DefoldPlatform.params[param] || 0; },

  dmNativeSetWindowSizeCallback: function(callback) { DefoldPlatform.callbacks.resize = callback; },
  dmNativeSetWindowFocusCallback: function(callback) { DefoldPlatform.callbacks.focus = callback; },

  dmNativeGetKey: function(key) { return DefoldPlatform.keys[key] || 0; },
  dmNativeGetMouseButton: function(button) {
    return button >= 0 && button < 8 && !!(DefoldPlatform.buttons & (1 << button));
  },
  dmNativeIsMouseLeftButtonFromTouch: function() { return DefoldPlatform.mouseLeftButtonFromTouch; },
  dmNativeIsMousePositionFromTouch: function() { return DefoldPlatform.mousePositionFromTouch; },
  dmNativeGetMousePos: function(x, y) {
    if (x) setValue(x, Browser.mouseX, 'i32');
    if (y) setValue(y, Browser.mouseY, 'i32');
  },
  dmNativeGetMouseWheel: function() { return DefoldPlatform.wheelPos; },

  dmNativeSetCursorVisible: function(visible) {
    var canvas = Module['canvas'];
    if (visible) {
      if (canvas && document.pointerLockElement === canvas && document.exitPointerLock) document.exitPointerLock();
    } else if (canvas && canvas.requestPointerLock && !DefoldPlatform.isPointerLocked) {
      canvas.requestPointerLock();
    }
  },

  dmNativeGetMouseLocked: function() { return Number(DefoldPlatform.isPointerLocked); },
  dmNativeSetCharCallback: function(callback) {
    DefoldPlatform.callbacks.character = callback;
    return 1;
  },
  dmNativeSetGamepadCallback: function(callback) {
    DefoldPlatform.callbacks.gamepad = callback;
    try {
      DefoldPlatform.refreshJoysticks(true);
      return 1;
    } catch (error) {
      console.error(error);
      DefoldPlatform.callbacks.gamepad = null;
      return 0;
    }
  },

  dmNativeGetJoystickParam: function(slot, param) {
    var known = DefoldPlatform.joys[slot];
    if (!known) return 0;
    switch (param) {
      case 0x50001: return 1;
      case 0x50002: return known.sample.axes.length;
      case 0x50003: return known.sample.buttons.length;
      default: return 0;
    }
  },

  dmNativeGetJoystickPos: function(slot, output, capacity) {
    DefoldPlatform.refreshJoysticks();
    var known = DefoldPlatform.joys[slot];
    var count = known ? Math.min(capacity, known.sample.axes.length) : 0;
    for (var i = 0; i < capacity; ++i) setValue(output + 4 * i, i < count ? known.sample.axes[i] : 0, 'float');
    return count;
  },

  dmNativeGetJoystickButtons: function(slot, output, capacity) {
    DefoldPlatform.refreshJoysticks();
    var known = DefoldPlatform.joys[slot];
    var count = known ? Math.min(capacity, known.sample.buttons.length) : 0;
    for (var i = 0; i < capacity; ++i) setValue(output + i, i < count && known.sample.buttons[i].pressed, 'i8');
    return count;
  },

  dmNativeGetJoystickDeviceId: function(slot, output) {
    var known = DefoldPlatform.joys[slot];
    setValue(output, known ? known.id : 0, '*');
    return known ? 1 : 0;
  },
  dmNativeGetJoystickDeviceGuid: function(slot, output) {
    var known = DefoldPlatform.joys[slot];
    setValue(output, known ? known.guid : 0, '*');
    return known ? 1 : 0;
  },

  dmNativeShowKeyboard: function(show_keyboard) {
    Module['canvas'].contentEditable = show_keyboard ? true : false;
    if (show_keyboard) {
      Module['canvas'].focus();
    }
  },

  dmNativeSetTouchCallback: function(cbfun) {
    DefoldPlatform.callbacks.touch = cbfun;
    return 1;
  },

  dmNativeGetDisplayScaleFactor: function() {
    // Match the canvas rendering ratio so pixel-ratio limits do not change camera zoom.
    return Module['getPixelRatio'](DefoldPlatform.params[21]); // WINDOW_STATE_HIGH_DPI
  },

  dmNativeGetDisplayDensity: function() {
    // A CSS pixel is 1/96 inch, so the device pixel ratio converts to dots per inch directly.
    return (window.devicePixelRatio || 1) * 96;
  }
};

autoAddDeps(LibraryDefoldPlatform, '$DefoldPlatform');
addToLibrary(LibraryDefoldPlatform);
