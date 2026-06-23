var LibrarySoundDevice =
{
    $DefoldSoundDevice: {
        TryResumeAudio: function() {
            if (typeof window !== "undefined" && window._dmJSDeviceShared) {
                var audioCtx = window._dmJSDeviceShared.audioCtx;
                if (audioCtx !== undefined && audioCtx.state != "running") {
                    audioCtx.resume();
                }
            }
        }
    },
    dmDeviceJSOpen: function(bufferCount) {
        if (typeof window === "undefined" || (!window.AudioContext && !window.webkitAudioContext)) {
            return -1;
        }

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
                    shared.audioCtx = new audioCtxCtor({ sampleRate: 48000 });
                } catch (e) {
                    // Fallback if the specified sampleRate isn't supported by the browser.
                    shared.audioCtx = new audioCtxCtor();
                }
            }

            var maxAdaptiveQueueSeconds = 0.250;
            var outputLatencyQueueScale = 0.5;
            var stablePlaybackSecondsBeforeDecay = 3.0;
            var queueDecayBufferRate = 0.5;

            // Construct web audio device.
            device = {
                sampleRate: shared.audioCtx.sampleRate,
                bufferedTo: 0,
                bufferDuration: 0,
                effectiveBufferCount: bufferCount,
                latencyQueueFloorSeconds: 0,
                learnedQueueFloorSeconds: 0,
                adaptiveQueueSeconds: 0,
                lastUnderrunTime: 0,
                lastQueueDecayTime: 0,
                ignoreNextUnderrun: false,
                copyToChannelNeedsSlice: null,
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
                // Cache whether WebAudio needs a non-shared copy from WASM memory.
                _needsCopyToChannelSlice: function(heapBuffer) {
                    if (this.copyToChannelNeedsSlice === null) {
                        this.copyToChannelNeedsSlice = typeof SharedArrayBuffer !== "undefined" && heapBuffer instanceof SharedArrayBuffer;
                    }
                    return this.copyToChannelNeedsSlice;
                },
                // Disabled-by-default trace hook for tuning browser-specific queue behavior.
                _debugLogQueueResize: function(reason, previousBufferCount, newBufferCount, previousSeconds, seconds) {
                    // Uncomment while tuning browser-specific HTML5 audio queue behavior.
                    /*
                    var floorSeconds = this._getAdaptiveQueueFloorSeconds();
                    console.log("sound queue resize (" + reason + "): buffers " +
                        previousBufferCount + " -> " + newBufferCount +
                        ", seconds " + previousSeconds.toFixed(3) + " -> " +
                        seconds.toFixed(3) +
                        ", latency floor " + this.latencyQueueFloorSeconds.toFixed(3) +
                        ", floor " + floorSeconds.toFixed(3) +
                        ", learned " + this.learnedQueueFloorSeconds.toFixed(3) +
                        ", buffer " + this.bufferDuration.toFixed(3));
                    */
                },
                // Restart the stable-playback timer after underruns or context state changes.
                _resetQueueDecay: function(audioTime) {
                    this.lastUnderrunTime = audioTime;
                    this.lastQueueDecayTime = audioTime;
                },
                // Convert a target queue duration into the buffer count reported to native code.
                _queueSecondsToBufferCount: function(seconds) {
                    if (this.bufferDuration <= 0) {
                        return bufferCount;
                    }
                    return Math.max(
                        bufferCount,
                        Math.ceil(seconds / this.bufferDuration)
                    );
                },
                // Keep the queue from decaying below browser latency or observed underrun needs.
                _getAdaptiveQueueFloorSeconds: function() {
                    return Math.max(
                        this.latencyQueueFloorSeconds,
                        this.learnedQueueFloorSeconds
                    );
                },
                // Recompute the effective buffer count used by _freeBufferSlots().
                _updateEffectiveBufferCount: function(reason, previousAdaptiveQueueSeconds) {
                    var previousEffectiveBufferCount = this.effectiveBufferCount;
                    if (this.bufferDuration <= 0) {
                        this.effectiveBufferCount = bufferCount;
                        return;
                    }

                    this.adaptiveQueueSeconds = Math.min(this.adaptiveQueueSeconds, maxAdaptiveQueueSeconds);
                    this.effectiveBufferCount = this._queueSecondsToBufferCount(this.adaptiveQueueSeconds);
                    if (this.effectiveBufferCount != previousEffectiveBufferCount) {
                        this._debugLogQueueResize(
                            reason,
                            previousEffectiveBufferCount,
                            this.effectiveBufferCount,
                            previousAdaptiveQueueSeconds,
                            this.adaptiveQueueSeconds
                        );
                    }
                },
                // Raise the queue target from browser-reported latency without shrinking it.
                _updateQueueTarget: function() {
                    var audioCtx = shared.audioCtx;
                    var reportedLatency = Math.max(audioCtx.outputLatency || 0, audioCtx.baseLatency || 0);
                    var latencyQueueSeconds = reportedLatency > 0 ? reportedLatency * outputLatencyQueueScale : 0;
                    var targetQueueSeconds = Math.min(
                        Math.max(bufferCount * this.bufferDuration, latencyQueueSeconds),
                        maxAdaptiveQueueSeconds
                    );
                    this.latencyQueueFloorSeconds = Math.max(this.latencyQueueFloorSeconds, targetQueueSeconds);

                    var previousAdaptiveQueueSeconds = this.adaptiveQueueSeconds;
                    this.adaptiveQueueSeconds = Math.max(
                        this.adaptiveQueueSeconds,
                        this._getAdaptiveQueueFloorSeconds()
                    );
                    this._updateEffectiveBufferCount("target", previousAdaptiveQueueSeconds);
                },
                // Increase the queue after active-playback starvation and remember the new floor.
                _growQueueAfterUnderrun: function(audioTime) {
                    this._resetQueueDecay(audioTime);
                    if (this.bufferDuration <= 0) {
                        return;
                    }

                    var previousEffectiveBufferCount = this.effectiveBufferCount;
                    var previousAdaptiveQueueSeconds = this.adaptiveQueueSeconds;
                    var previousFloorSeconds = this._getAdaptiveQueueFloorSeconds();
                    var previousFloorBufferCount = this._queueSecondsToBufferCount(previousFloorSeconds);

                    this.learnedQueueFloorSeconds = Math.min(
                        maxAdaptiveQueueSeconds,
                        Math.max(
                            this.learnedQueueFloorSeconds,
                            (previousEffectiveBufferCount + 1) * this.bufferDuration
                        )
                    );

                    var floorSeconds = this._getAdaptiveQueueFloorSeconds();
                    if (floorSeconds != previousFloorSeconds) {
                        this._debugLogQueueResize(
                            "floor",
                            previousFloorBufferCount,
                            this._queueSecondsToBufferCount(floorSeconds),
                            previousFloorSeconds,
                            floorSeconds
                        );
                    }

                    this.adaptiveQueueSeconds = Math.min(
                        maxAdaptiveQueueSeconds,
                        Math.max(
                            this.adaptiveQueueSeconds * 1.5,
                            this.adaptiveQueueSeconds + this.bufferDuration
                        )
                    );
                    this._updateEffectiveBufferCount("underrun", previousAdaptiveQueueSeconds);
                },
                // Slowly reduce the queue after sustained playback, down to the current floor.
                _decayQueueAfterStablePlayback: function(audioTime) {
                    var floorSeconds = this._getAdaptiveQueueFloorSeconds();
                    if (this.bufferDuration <= 0 || this.adaptiveQueueSeconds <= floorSeconds) {
                        return;
                    }

                    if (audioTime - this.lastUnderrunTime < stablePlaybackSecondsBeforeDecay) {
                        this.lastQueueDecayTime = audioTime;
                        return;
                    }

                    var elapsed = audioTime - this.lastQueueDecayTime;
                    if (elapsed <= 0) {
                        return;
                    }

                    var decay = elapsed * this.bufferDuration * queueDecayBufferRate;
                    var previousAdaptiveQueueSeconds = this.adaptiveQueueSeconds;
                    this.adaptiveQueueSeconds = Math.max(
                        floorSeconds,
                        this.adaptiveQueueSeconds - decay
                    );
                    this.lastQueueDecayTime = audioTime;
                    this._updateEffectiveBufferCount("decay", previousAdaptiveQueueSeconds);
                },
                _queue: function(samples, frame_count) {
                    var lastBufferDuration = this.bufferDuration;
                    var len = frame_count / this.sampleRate;
                    // use real buffer length next time.
                    this.bufferDuration = len;

                    this._updateQueueTarget();

                    // only append overall length of audio buffer in suspended stay
                    // it helps prevent sound instance consume on the engine side
                    // because from engine point of view - sound plays.
                    if (!this._isContextRunning()) {
                        this.suspendedBufferedTo += len;
                        this.ignoreNextUnderrun = true;
                        return;
                    }

                    // Setup buffer for data delivery...
                    var buf = shared.audioCtx.createBuffer(2, frame_count, this.sampleRate);

                    // Copy data from WASM memory
                    var heapBuffer = HEAPF32.buffer;
                    var needsCopy = this._needsCopyToChannelSlice(heapBuffer);
                    for(var c=0;c<2;c++) {
                        var input = new Float32Array(heapBuffer, samples, frame_count);
                        // copyToChannel cannot handle SharedArrayBuffer views, so threaded builds need a non-shared copy.
                        buf.copyToChannel(needsCopy ? input.slice() : input, c);
                        samples += frame_count * 4; // 4 bytes = sizeof(float)
                    }
                    var source = shared.audioCtx.createBufferSource();
                    source.buffer = buf;
                    source.connect(shared.audioCtx.destination);

                    var t = shared.audioCtx.currentTime;
                    var startTime;
                    var firstBuffer = lastBufferDuration == 0.0;
                    var bufferedToSeconds = this.bufferedTo / this.sampleRate;
                    var underrun = !firstBuffer && bufferedToSeconds <= t;

                    if (underrun) {
                        // Suppress expected gaps after suspend/resume. Other underruns are
                        // active-playback starvation and should grow the adaptive queue.
                        if (this.ignoreNextUnderrun) {
                            this.ignoreNextUnderrun = false;
                            this._resetQueueDecay(t);
                        } else {
                            this._growQueueAfterUnderrun(t);
                        }
                    } else {
                        this.ignoreNextUnderrun = false;
                        if (!firstBuffer) {
                            this._decayQueueAfterStablePlayback(t);
                        }
                    }

                    // Underrun or first buffer?
                    if (underrun || firstBuffer) {
                        // Yes, restart buffering - offset is always computed based on queue length...
                        var off = (this.effectiveBufferCount - 1) * this.bufferDuration;
                        startTime = t + off;
                        this.bufferedTo = startTime * this.sampleRate;
                    } else {
                        // No, normal delivery...
                        startTime = this.bufferedTo / this.sampleRate;
                    }
                    source.start(startTime);
                    this.bufferedTo += frame_count;
                },
                _freeBufferSlots: function() {
                    var ahead = 0;
                    if (this._isContextRunning()) {
                        // before knowing the length of each buffer, we return a dummy count to enable initial delivery
                        if (this.bufferDuration == 0)
                            return 1;
                        ahead = this.bufferedTo / this.sampleRate - shared.audioCtx.currentTime;
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
                    var left = this.effectiveBufferCount - inqueue;
                    if (left < 0) {
                        return 0;
                    }
                    return left;
                }
            };
        }

        if (device != null) {
            shared.audioCtx.onstatechange = function() {
                var audioTime = shared.audioCtx.currentTime || 0;
                device._resetQueueDecay(audioTime);
                if (device._isContextRunning()) {
                    device.timeInSuspendedState = Date.now() / 1000;
                    device.ignoreNextUnderrun = true;
                } else {
                    // reset all counters for suspended or closed state
                    device.creatingTime = Date.now() / 1000;
                    device.lastTimeInSuspendedState = Date.now() / 1000;
                    device.suspendedBufferedTo = 0;
                    device.ignoreNextUnderrun = true;
                }
            };
            shared.devices[id] = device;
            return id;
        }
        return -1;
    },
    dmDeviceJSOpen__proxy: 'sync',
    dmDeviceJSOpen__sig: 'ii',

    dmDeviceJSQueue: function(id, samples, sample_count) {
        window._dmJSDeviceShared.devices[id]._queue(samples, sample_count)
    },
    dmDeviceJSQueue__proxy: 'sync',
    dmDeviceJSQueue__sig: 'viii',

    dmDeviceJSFreeBufferSlots: function(id) {
        return window._dmJSDeviceShared.devices[id]._freeBufferSlots();
    },
    dmDeviceJSFreeBufferSlots__proxy: 'sync',
    dmDeviceJSFreeBufferSlots__sig: 'ii',

    dmGetDeviceSampleRate: function(id) {
        return window._dmJSDeviceShared.devices[id].sampleRate;
    },
}

autoAddDeps(LibrarySoundDevice, '$DefoldSoundDevice');
addToLibrary(LibrarySoundDevice);
