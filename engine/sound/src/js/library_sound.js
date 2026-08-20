var LibrarySoundDevice =
{
    $DefoldSoundDevice: {
        GetGlobal: function() {
            if (typeof globalThis !== "undefined") {
                return globalThis;
            }
            if (typeof window !== "undefined") {
                return window;
            }
            if (typeof self !== "undefined") {
                return self;
            }
            return undefined;
        },
        Now: function() {
            return Date.now() / 1000;
        },
        ForEachDevice: function(shared, callback) {
            for (var id in shared.devices) {
                if (Object.prototype.hasOwnProperty.call(shared.devices, id)) {
                    callback(shared.devices[id]);
                }
            }
        },
        HasDevices: function(shared) {
            for (var id in shared.devices) {
                if (Object.prototype.hasOwnProperty.call(shared.devices, id)) {
                    return true;
                }
            }
            return false;
        },
        WarnSilentFallback: function(shared) {
            shared.openedSilently = true;
            if (!shared.fallbackLogged && typeof console !== "undefined" && console.warn) {
                console.warn("DEFOLD: Web Audio is unavailable. Continuing with silent audio and retrying every 2 seconds.");
                shared.fallbackLogged = true;
            }
        },
        NotifyContextStateChanged: function(shared) {
            var audioCtx = shared.audioCtx;
            var running = audioCtx !== undefined && audioCtx.state == "running";
            // Retries can report the same state repeatedly. Reset device queues only on a real transition.
            if (running != shared.contextRunning) {
                shared.contextRunning = running;
                DefoldSoundDevice.ForEachDevice(shared, function(device) {
                    device._onContextStateChanged(running);
                });
            }

            if (running) {
                if (shared.retryTimer !== null) {
                    var root = DefoldSoundDevice.GetGlobal();
                    if (root !== undefined && root.clearTimeout) {
                        root.clearTimeout(shared.retryTimer);
                    }
                    shared.retryTimer = null;
                }
                if (shared.openedSilently && !shared.recoveryLogged && typeof console !== "undefined" && console.info) {
                    console.info("DEFOLD: Web Audio became available. Switching from silent audio to normal output.");
                    shared.recoveryLogged = true;
                }
            } else {
                DefoldSoundDevice.ScheduleAudioRetry(shared);
            }
        },
        HandleAudioFailure: function(shared, failedContext) {
            if (failedContext !== undefined && shared.audioCtx !== failedContext) {
                return;
            }

            var audioCtx = shared.audioCtx;
            // Detach the failed context first so every device immediately switches to silent clocking.
            shared.audioCtx = undefined;
            shared.resumePending = false;
            if (audioCtx !== undefined) {
                audioCtx.onstatechange = null;
                if (audioCtx.state != "closed" && audioCtx.close) {
                    try {
                        var closeResult = audioCtx.close();
                        if (closeResult && closeResult.catch) {
                            closeResult.catch(function() {});
                        }
                    } catch (e) {
                    }
                }
            }

            DefoldSoundDevice.WarnSilentFallback(shared);
            DefoldSoundDevice.NotifyContextStateChanged(shared);
        },
        TryCreateAudioContext: function(shared) {
            if (shared.audioCtx !== undefined && shared.audioCtx.state != "closed") {
                return shared.audioCtx;
            }

            shared.audioCtx = undefined;
            var root = DefoldSoundDevice.GetGlobal();
            var audioCtxCtor = root !== undefined ? (root.AudioContext || root.webkitAudioContext) : undefined;
            if (audioCtxCtor === undefined) {
                return undefined;
            }

            var audioCtx;
            try {
                // The default sample rate varies by output device and can be lower than 44100 Hz.
                audioCtx = new audioCtxCtor({ sampleRate: 48000 });
            } catch (e) {
                try {
                    // Some browsers do not support requesting a sample rate in the constructor.
                    audioCtx = new audioCtxCtor();
                } catch (fallbackError) {
                    return undefined;
                }
            }

            if (audioCtx === undefined || audioCtx.state == "closed") {
                return undefined;
            }

            shared.audioCtx = audioCtx;
            shared.resumePending = false;
            audioCtx.onstatechange = function() {
                if (shared.audioCtx !== audioCtx) {
                    return;
                }
                if (audioCtx.state == "closed") {
                    DefoldSoundDevice.HandleAudioFailure(shared, audioCtx);
                } else {
                    DefoldSoundDevice.NotifyContextStateChanged(shared);
                }
            };
            DefoldSoundDevice.NotifyContextStateChanged(shared);
            return audioCtx;
        },
        TryResumeSharedAudio: function(shared, forceResume) {
            var audioCtx = DefoldSoundDevice.TryCreateAudioContext(shared);
            if (audioCtx === undefined) {
                DefoldSoundDevice.WarnSilentFallback(shared);
                DefoldSoundDevice.ScheduleAudioRetry(shared);
                return;
            }

            if (audioCtx.state != "running" && audioCtx.resume && (!shared.resumePending || forceResume)) {
                try {
                    var resumeResult = audioCtx.resume();
                    if (resumeResult && resumeResult.then) {
                        // Interaction may supersede an earlier pending resume. Ignore results from stale attempts.
                        var resumeAttempt = ++shared.resumeAttempt;
                        shared.resumePending = true;
                        resumeResult.then(function() {
                            if (shared.audioCtx !== audioCtx || shared.resumeAttempt != resumeAttempt) {
                                return;
                            }
                            shared.resumePending = false;
                            DefoldSoundDevice.NotifyContextStateChanged(shared);
                        }, function() {
                            if (shared.audioCtx !== audioCtx || shared.resumeAttempt != resumeAttempt) {
                                return;
                            }
                            shared.resumePending = false;
                            if (audioCtx.state == "closed") {
                                DefoldSoundDevice.HandleAudioFailure(shared, audioCtx);
                            } else {
                                DefoldSoundDevice.ScheduleAudioRetry(shared);
                            }
                        });
                    }
                } catch (e) {
                    DefoldSoundDevice.HandleAudioFailure(shared, audioCtx);
                    return;
                }
            }

            if (audioCtx.state == "running") {
                DefoldSoundDevice.NotifyContextStateChanged(shared);
            } else {
                DefoldSoundDevice.ScheduleAudioRetry(shared);
            }
        },
        ScheduleAudioRetry: function(shared) {
            if (shared.retryTimer !== null || !DefoldSoundDevice.HasDevices(shared)) {
                return;
            }

            var root = DefoldSoundDevice.GetGlobal();
            if (root === undefined || !root.setTimeout) {
                return;
            }

            shared.retryTimer = root.setTimeout(function() {
                // One shared timer services every Defold sound device on the page.
                shared.retryTimer = null;
                if (!DefoldSoundDevice.HasDevices(shared)) {
                    return;
                }
                DefoldSoundDevice.TryResumeSharedAudio(shared, false);
            }, 2000);
        },
        InstallDeviceChangeListener: function(shared) {
            if (shared.deviceChangeHandler !== null) {
                return;
            }

            var root = DefoldSoundDevice.GetGlobal();
            var mediaDevices = root !== undefined && root.navigator ? root.navigator.mediaDevices : undefined;
            if (mediaDevices && mediaDevices.addEventListener) {
                shared.deviceChangeHandler = function() {
                    DefoldSoundDevice.TryResumeSharedAudio(shared, true);
                };
                mediaDevices.addEventListener("devicechange", shared.deviceChangeHandler);
            }
        },
        RemoveDeviceChangeListener: function(shared) {
            if (shared.deviceChangeHandler === null) {
                return;
            }

            var root = DefoldSoundDevice.GetGlobal();
            var mediaDevices = root !== undefined && root.navigator ? root.navigator.mediaDevices : undefined;
            if (mediaDevices && mediaDevices.removeEventListener) {
                mediaDevices.removeEventListener("devicechange", shared.deviceChangeHandler);
            }
            shared.deviceChangeHandler = null;
        },
        TryResumeAudio: function() {
            var root = DefoldSoundDevice.GetGlobal();
            if (root !== undefined && root._dmJSDeviceShared) {
                DefoldSoundDevice.TryResumeSharedAudio(root._dmJSDeviceShared, true);
            }
        }
    },
    dmDeviceJSOpen: function(bufferCount) {
        var root = DefoldSoundDevice.GetGlobal();
        if (root === undefined) {
            return -1;
        }

        // globally shared data
        var shared = root._dmJSDeviceShared;
        if (shared === undefined) {
            shared = {
                count: 0,
                devices: {},
                audioCtx: undefined,
                retryTimer: null,
                deviceChangeHandler: null,
                fallbackLogged: false,
                recoveryLogged: false,
                openedSilently: false,
                contextRunning: false,
                resumePending: false,
                resumeAttempt: 0
            };
            root._dmJSDeviceShared = shared;
        }

        var id = shared.count++;
        var audioCtx = DefoldSoundDevice.TryCreateAudioContext(shared);
        // Keep this rate for the device lifetime. A recovered Web Audio context resamples if necessary.
        var sampleRate = audioCtx !== undefined ? audioCtx.sampleRate : 48000;
        if (audioCtx === undefined) {
            DefoldSoundDevice.WarnSilentFallback(shared);
        }

        var maxAdaptiveQueueSeconds = 0.250;
        var outputLatencyQueueScale = 0.5;
        var stablePlaybackSecondsBeforeDecay = 3.0;
        var queueDecayBufferRate = 0.5;

        // Construct a device that uses Web Audio when it is running and a clocked silent queue otherwise.
        var device = {
            sampleRate: sampleRate,
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
            activeSources: [],
            creatingTime: DefoldSoundDevice.Now(),
            lastTimeInSuspendedState: DefoldSoundDevice.Now(),
            suspendedBufferedTo: 0,
            // functions
            _isContextRunning: function() {
                var audioCtx = shared.audioCtx;
                return audioCtx !== undefined && audioCtx.state == "running";
            },
            _getCurrentSuspendedTime: function() {
                if (!this._isContextRunning()) {
                    this.lastTimeInSuspendedState = DefoldSoundDevice.Now();
                    return this.lastTimeInSuspendedState - this.creatingTime;
                }
                return 0;
            },
            _onContextStateChanged: function(running) {
                var now = DefoldSoundDevice.Now();
                var audioTime = running && shared.audioCtx !== undefined ? (shared.audioCtx.currentTime || 0) : 0;
                if (!running) {
                    // The native mixer keeps advancing silently, so queued Web Audio must not replay on recovery.
                    this._cancelActiveSources();
                }
                // Change clocks without resetting native sound instances or their logical playback position.
                this._resetQueueDecay(audioTime);
                this.creatingTime = now;
                this.lastTimeInSuspendedState = now;
                this.suspendedBufferedTo = 0;
                this.ignoreNextUnderrun = true;
                if (running) {
                    // Force the first real buffer to start from the recovered context's current time.
                    this.bufferedTo = 0;
                }
            },
            _cancelActiveSources: function() {
                for (var i = 0; i < this.activeSources.length; ++i) {
                    var source = this.activeSources[i];
                    source.onended = null;
                    try {
                        source.stop();
                    } catch (e) {
                    }
                    try {
                        source.disconnect();
                    } catch (e) {
                    }
                }
                this.activeSources.length = 0;
            },
            _trackActiveSource: function(source) {
                var device = this;
                // Keep scheduled sources only so context transitions and device close can cancel them.
                this.activeSources.push(source);
                source.onended = function() {
                    var index = device.activeSources.indexOf(source);
                    if (index != -1) {
                        device.activeSources.splice(index, 1);
                    }
                };
            },
            _queueSilently: function(bufferDuration) {
                var suspendedTime = this._getCurrentSuspendedTime();
                // After an idle gap, start at wall-clock time instead of extending an expired queue.
                this.suspendedBufferedTo = Math.max(this.suspendedBufferedTo, suspendedTime) + bufferDuration;
                this.ignoreNextUnderrun = true;
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
                if (audioCtx === undefined) {
                    return;
                }
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
            _markPlaybackStarted: function() {
                var audioTime = this._isContextRunning() ? (shared.audioCtx.currentTime || 0) : 0;
                this.ignoreNextUnderrun = true;
                this._resetQueueDecay(audioTime);
            },
            _markPlaybackIdle: function() {
                // Mark idle so the next start doesn't learn the stale queue gap as an underrun.
                // Already scheduled WebAudio buffers are left to finish naturally.
                var audioTime = this._isContextRunning() ? (shared.audioCtx.currentTime || 0) : 0;
                this.ignoreNextUnderrun = true;
                this._resetQueueDecay(audioTime);
            },
            _queue: function(samples, frame_count) {
                var lastBufferDuration = this.bufferDuration;
                var len = frame_count / this.sampleRate;
                // use real buffer length next time.
                this.bufferDuration = len;

                // Advance a wall-clock queue while Web Audio is unavailable or autoplay-suspended.
                // This lets the native mixer reach end-of-stream and recycle sound instances.
                if (!this._isContextRunning()) {
                    this._queueSilently(len);
                    return;
                }

                var audioCtx = shared.audioCtx;
                try {
                    this._updateQueueTarget();

                    // Setup buffer for data delivery...
                    var buf = audioCtx.createBuffer(2, frame_count, this.sampleRate);

                    // Copy data from WASM memory
                    var heapBuffer = HEAPF32.buffer;
                    var needsCopy = this._needsCopyToChannelSlice(heapBuffer);
                    for(var c=0;c<2;c++) {
                        var input = new Float32Array(heapBuffer, samples, frame_count);
                        // copyToChannel cannot handle SharedArrayBuffer views, so threaded builds need a non-shared copy.
                        buf.copyToChannel(needsCopy ? input.slice() : input, c);
                        samples += frame_count * 4; // 4 bytes = sizeof(float)
                    }
                    var source = audioCtx.createBufferSource();
                    source.buffer = buf;
                    source.connect(audioCtx.destination);
                    this._trackActiveSource(source);

                    var t = audioCtx.currentTime;
                    var startTime;
                    var firstBuffer = lastBufferDuration == 0.0;
                    var bufferedToSeconds = this.bufferedTo / this.sampleRate;
                    var underrun = !firstBuffer && bufferedToSeconds <= t;

                    if (underrun) {
                        // Suppress expected gaps after suspend/resume or an idle restart.
                        // Other underruns are active-playback starvation and should grow the adaptive queue.
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
                } catch (e) {
                    DefoldSoundDevice.HandleAudioFailure(shared, audioCtx);
                    // Account for the failed real buffer once so native playback continues at the same rate.
                    this._queueSilently(len);
                }
            },
            _freeBufferSlots: function() {
                // Always permit the first mix buffer so bufferDuration becomes known in silent mode too.
                if (this.bufferDuration <= 0) {
                    return 1;
                }

                var ahead = 0;
                if (this._isContextRunning()) {
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

        shared.devices[id] = device;
        DefoldSoundDevice.InstallDeviceChangeListener(shared);
        DefoldSoundDevice.NotifyContextStateChanged(shared);
        return id;
    },
    dmDeviceJSOpen__proxy: 'sync',
    dmDeviceJSOpen__sig: 'ii',

    dmDeviceJSClose: function(id) {
        var root = DefoldSoundDevice.GetGlobal();
        var shared = root !== undefined ? root._dmJSDeviceShared : undefined;
        if (shared === undefined || shared.devices[id] === undefined) {
            return;
        }

        shared.devices[id]._cancelActiveSources();
        delete shared.devices[id];
        if (DefoldSoundDevice.HasDevices(shared)) {
            return;
        }

        // The context and recovery hooks are shared, so release them only after the final device closes.
        if (shared.retryTimer !== null && root.clearTimeout) {
            root.clearTimeout(shared.retryTimer);
            shared.retryTimer = null;
        }
        DefoldSoundDevice.RemoveDeviceChangeListener(shared);

        var audioCtx = shared.audioCtx;
        shared.audioCtx = undefined;
        if (audioCtx !== undefined) {
            audioCtx.onstatechange = null;
            if (audioCtx.state != "closed" && audioCtx.close) {
                try {
                    var closeResult = audioCtx.close();
                    if (closeResult && closeResult.catch) {
                        closeResult.catch(function() {});
                    }
                } catch (e) {
                }
            }
        }
        delete root._dmJSDeviceShared;
    },
    dmDeviceJSClose__proxy: 'sync',
    dmDeviceJSClose__sig: 'vi',

    dmDeviceJSPlaybackStarted: function(id) {
        var shared = DefoldSoundDevice.GetGlobal()._dmJSDeviceShared;
        shared.devices[id]._markPlaybackStarted();
    },
    dmDeviceJSPlaybackStarted__proxy: 'sync',
    dmDeviceJSPlaybackStarted__sig: 'vi',

    dmDeviceJSPlaybackIdle: function(id) {
        var shared = DefoldSoundDevice.GetGlobal()._dmJSDeviceShared;
        shared.devices[id]._markPlaybackIdle();
    },
    dmDeviceJSPlaybackIdle__proxy: 'sync',
    dmDeviceJSPlaybackIdle__sig: 'vi',

    dmDeviceJSQueue: function(id, samples, sample_count) {
        var shared = DefoldSoundDevice.GetGlobal()._dmJSDeviceShared;
        shared.devices[id]._queue(samples, sample_count);
    },
    dmDeviceJSQueue__proxy: 'sync',
    dmDeviceJSQueue__sig: 'viii',

    dmDeviceJSFreeBufferSlots: function(id) {
        var shared = DefoldSoundDevice.GetGlobal()._dmJSDeviceShared;
        return shared.devices[id]._freeBufferSlots();
    },
    dmDeviceJSFreeBufferSlots__proxy: 'sync',
    dmDeviceJSFreeBufferSlots__sig: 'ii',

    dmGetDeviceSampleRate: function(id) {
        var shared = DefoldSoundDevice.GetGlobal()._dmJSDeviceShared;
        return shared.devices[id].sampleRate;
    },
}

autoAddDeps(LibrarySoundDevice, '$DefoldSoundDevice');
addToLibrary(LibrarySoundDevice);
