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
                sampleRate: 44100,
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
            
        } else {
            // Create flash audio device
            if (document.getElementById('DEFOLD_FLASH_SND') == null) {
                var el = document.createElement('div');
                el.id = 'DEFOLD_FLASH_SND';
                el.setAttribute("id", "DEFOLD_FLASH_SND");
                el.setAttribute("style","background:#ff00ff;position:static;");
                var body = document.getElementsByTagName("BODY");
                body[0].appendChild(el);
                el.innerHTML = "<div style=\"position:fixed;right:0px;bottom:0px\"> <object id=\"defold_sound_swf\" style=\"display: block;\" classid=\"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000\" codebase=\"http:\/\/download.macromedia.com\/pub\/shockwave\/cabs\/flash\/swflash.cab#version=4,0,0,0\" width=\"1\" height=\"1\"><param name=\"movie\" value=\"defold_sound.swf\"><param name=\"LOOP\" value=\"false\"><param name=\"quality\" value=\"high\"><param name=\"allowScriptAccess\" value=\"always\"><embed src=\"defold_sound.swf\" width=\"1\" height=\"1\" loop=\"false\" quality=\"high\" pluginspage=\"http:\/\/www.macromedia.com\/shockwave\/download\/index.cgi?P1_Prod_Version=ShockwaveFlash\" type=\"application\/x-shockwave-flash\" allowscriptaccess=\"always\"><\/object><\/div>";
            }
            device = {
                buffered: [],
                bufferSize: 0,
                flashMinSize: 4096,
                _queue: function(samples, sample_count) {
                    var s = "";
                    for (var i=0;i<2*sample_count;i++) {
                        s += String.fromCharCode(32768 + getValue(samples+2*i, 'i16') / 2);
                    }
                    var l = this.buffered.length - 1;
                    if (this.buffered.length > 0 && this.buffered[l].length < this.flashMinSize) {
                        // there is a buffer that is not big enough yet.
                        this.buffered[l] = this.buffered[l] + s;
                    } else {
                        this.buffered.push(s);
                    }
                    this.bufferSize = 2 * sample_count;
                },
                _freeBufferSlots: function() {
                    if (this.bufferSize == 0)
                        return 1;
                    var tot = 0;
                    for (var k=0;k<this.buffered.length;k++)
                        tot = tot + this.buffered[k].length;

                    // ignore the buffer count for flash; it does internal buffering and we need to ask for at least
                    // 2*4096 bytes which is a little more than typical buffer count * size gives.
                    var left = Math.floor((2 * this.flashMinSize - tot) / this.bufferSize);
                    if (left < 0)
                        return 0;
                    return left;
                }
            };
            window.dmFlashGetSoundData = function() {
                if (device.buffered.length > 0 && device.buffered[0].length >= device.flashMinSize) {
                    var buf = device.buffered.splice(0, 1)[0];
                    return buf;
                } else {
                    var s = "";
                    for (var i=0;i<device.flashMinSize;i++) 
                        s += String.fromCharCode(32768);
                    return s;
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
    }
}

autoAddDeps(LibrarySoundDevice, '$DefoldSoundDevice');
mergeInto(LibraryManager.library, LibrarySoundDevice);
