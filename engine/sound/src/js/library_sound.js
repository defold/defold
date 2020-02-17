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
                shared.audioCtx = new (window.AudioContext || window.webkitAudioContext)();
            }                
            // Construct web audio device.
            device = {
                sampleRate: shared.audioCtx.sampleRate,
                bufferedTo: 0,
                bufferDuration: 0,
                // functions
                _queue: function(samples, sample_count) {
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
                    var len = sample_count / this.sampleRate;
                    var t = shared.audioCtx.currentTime;
                    if (this.bufferedTo <= t) {
                        source.start(t);
                        this.bufferedTo = t + len;
                    } else {
                        source.start(this.bufferedTo);
                        this.bufferedTo = this.bufferedTo + len;
                    }
                    // use real buffer length next time.
                    this.bufferDuration = len;
                },
                _freeBufferSlots: function() {
                    // before knowing the length of each buffer.
                    if (this.bufferDuration == 0)
                        return 1;
                    var ahead = this.bufferedTo - shared.audioCtx.currentTime;
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
            shared.devices[id] = device;
            return id;
        } 
        return 0;
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
mergeInto(LibraryManager.library, LibrarySoundDevice);
