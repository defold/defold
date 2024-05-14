var LibrarySoundDevice = 
{
   $DefoldSoundDevice: {
      TryResumeAudio: function() {
         var audioCtx = window._dmJSDeviceShared.audioCtx;
         if (audioCtx !== undefined && audioCtx.state != "running") {
             audioCtx.resume();
         }
      }
   },
   dmDeviceJSOpen: function(bufferCount) {

        // globally shared data        
        var shared = window._dmJSDeviceShared;
        if (shared === undefined) {
            shared = { 
                count: 0,
                devices: {}
            };
            window._dmJSDeviceShared = shared;
        }

        var id = shared.count++;
        var device;

        if (window.AudioContext || window.webkitAudioContext) {
            if (shared.audioCtx === undefined) {
                var audioCtxCtor = window.AudioContext || window.webkitAudioContext;
                try {
                    // The default sampleRate varies depending on the output device and can be less than 44100.
                    shared.audioCtx = new audioCtxCtor({ sampleRate: 44100 });
                } catch (e) {
                    // Fallback if the specified `sampleRate` isn't supported by the browser.
                    shared.audioCtx = new audioCtxCtor();
                }
            }
            // Construct web audio device.
            device = {
                sampleRate: shared.audioCtx.sampleRate,
                bufferedTo: 0,
                bufferDuration: 0,
                creatingTime: Date.now() / 1000,
                lastTimeInSuspendedState: Date.now() / 1000,
                suspendedBufferedTo: 0,
                // functions
                _isContextRunning: function() {
                    var audioCtx = window._dmJSDeviceShared.audioCtx;
                    return audioCtx !== undefined && audioCtx.state == "running"
                },
                _getCurrentSuspendedTime: function() {
                    if (!this._isContextRunning()) {
                        this.lastTimeInSuspendedState = Date.now() / 1000;
                        return this.lastTimeInSuspendedState - this.creatingTime;
                    }
                    return 0;
                },
                _queue: function(samples, sample_count) {
                    var len = sample_count / this.sampleRate;
                    // use real buffer length next time.
                    this.bufferDuration = len;
                    // only append overall length of audio buffer in suspended stay
                    // it helps prevent sound instance consume on the engine side
                    // because from engine point of view - sound plays.
                    if (!this._isContextRunning()) {
                        this.suspendedBufferedTo += len;
                        return;
                    }
                    var buf = shared.audioCtx.createBuffer(2, sample_count, this.sampleRate);
                    var c0 = buf.getChannelData(0);
                    var c1 = buf.getChannelData(1);
                    for (var i=0;i<sample_count;i++) {
                        c0[i] = getValue(samples+4*i, 'i16') / 32768;
                        c1[i] = getValue(samples+4*i+2, 'i16') / 32768;
                    }
                    var source = shared.audioCtx.createBufferSource();
                    source.buffer = buf;
                    source.connect(shared.audioCtx.destination);
                    var t = shared.audioCtx.currentTime;
                    if (this.bufferedTo <= t) {
                        source.start(t);
                        this.bufferedTo = t + len;
                    } else {
                        source.start(this.bufferedTo);
                        this.bufferedTo = this.bufferedTo + len;
                    }
                },
                _freeBufferSlots: function() {
                    var ahead = 0;
                    if (this._isContextRunning()) {
                        // before knowing the length of each buffer.
                        if (this.bufferDuration == 0)
                            return 1;
                        ahead = this.bufferedTo - shared.audioCtx.currentTime;
                    } else {
                        // if audio context in suspended or closed state we simulate audio play
                        // by calculating play time
                        // when audio context become active - start filling audio buffer from the beginning
                        // and start using audioCtx.currentTime
                        ahead = this.suspendedBufferedTo - this._getCurrentSuspendedTime();
                    }
                    var inqueue = Math.ceil(ahead / this.bufferDuration);
                    if (inqueue < 0) {
                        inqueue = 0;
                    }
                    var left = bufferCount - inqueue;
                    if (left < 0) {
                        return 0;
                    }
                    return left;
                }
            };
        }
        
        if (device != null) {
            shared.audioCtx.onstatechanged = function() {
                if (device._isContextRunning()) {
                    device.timeInSuspendedState = Date.now() / 1000;
                } else {
                    // reset all counters for suspended or closed state
                    device.creatingTime = Date.now() / 1000;
                    device.lastTimeInSuspendedState = Date.now() / 1000;
                    device.suspendedBufferedTo = 0;
                }
            };
            shared.devices[id] = device;
            return id;
        } 
        return -1;
    },
    
    dmDeviceJSQueue: function(id, samples, sample_count) {
        window._dmJSDeviceShared.devices[id]._queue(samples, sample_count)
    },
    
    dmDeviceJSFreeBufferSlots: function(id) {
        return window._dmJSDeviceShared.devices[id]._freeBufferSlots();
    },

    dmGetDeviceSampleRate: function(id) {
        return window._dmJSDeviceShared.devices[id].sampleRate;
    }
}

autoAddDeps(LibrarySoundDevice, '$DefoldSoundDevice');
addToLibrary(LibrarySoundDevice);
