/*
*     'archive_location_filter':
*         Filter function that will run for each archive path, called as
*         filter(path, attempt) where attempt counts up per retry. What it returns is
*         requested as is. The default filter varies the url on retries with
*         FileLoader.addCacheBuster(url, attempt) unless an archive_location_suffix is
*         configured, since a suffix may carry a signature that covers the query string.
*         A custom filter that signs its urls must sign the retry url too, or ignore
*         attempt to reuse the signed url.
*
*     'unsupported_webgl_callback':
*         Function that is called if WebGL is not supported.
*
*     'engine_arguments':
*         List of arguments (strings) that will be passed to the engine.
*
*     'custom_heap_size':
*         Number of bytes specifying the memory heap size.
*
*     'disable_context_menu':
*         Disables the right-click context menu on the canvas element if true.
*
*     'retry_time':
*         Pause before retry file loading after error.
*
*     'retry_count':
*         How many attempts we do when trying to download a file.
*
*     'can_not_download_file_callback':
*         Function that is called if you can't download file after 'retry_count' attempts.
*
*     'exe_name':
*         Executable name which used for find right binary to load
*
*     'resize_window_callback':
*         Function called on resize, orientation, focus, fullscreen and device pixel ratio changes.
*
*     'start_success':
*         Function that is called just before main is called upon successful load.
*
*     'start_error':
*         Function that is called if startup fails or the engine aborts.
*
*     'update_progress':
*         Function that is called as progress is updated. Parameter progress is updated 0-100.
*
*     'update_imports':
*         Function that is called right before wasm instantiation. Imports
*         are passed into the function and can be modified and will be
*         subsequently passed on to WebAssembly.
*/
var CUSTOM_PARAMETERS = {
    archive_location_filter: function( path, attempt ) {
        var url = "{{DEFOLD_ARCHIVE_LOCATION_PREFIX}}" + path;
        var suffix = "{{DEFOLD_ARCHIVE_LOCATION_SUFFIX}}";
        // a configured suffix may carry a signature that covers the query string
        if (suffix) return url + suffix;
        return attempt ? FileLoader.addCacheBuster(url, attempt) : url;
    },
    engine_arguments: [{{#DEFOLD_ENGINE_ARGUMENTS}}"{{.}}",{{/DEFOLD_ENGINE_ARGUMENTS}}],
    custom_heap_size: {{DEFOLD_HEAP_SIZE}},
    full_screen_container: "#canvas-container",
    disable_context_menu: true,
    retry_time:{{html5.retry_time}},
    retry_count:{{html5.retry_count}},
    unsupported_webgl_callback: function() {
        var e = document.getElementById("webgl-not-supported");
        e.style.display = "block";
    },
    start_success: function() {
    },
    start_error: function(error) {
    },
    update_progress: function(progress) {
    },
    update_imports: function(imports) {
    },
    resize_window_prev_inner_width: -1,
    resize_window_prev_inner_height: -1,
    resize_window_prev_pixel_ratio: -1,
    resize_window_prev_fullscreen: false,
    resize_window_callback: function() {
        var is_iOS = /iPad|iPhone|iPod/.test(navigator.userAgent) && !window.MSStream;
        var buttonHeight = 0;
        {{#html5.show_made_with_defold}}
        buttonHeight = 42;
        {{/html5.show_made_with_defold}}
        {{#html5.show_fullscreen_button}}
        buttonHeight = 42;
        {{/html5.show_fullscreen_button}}
        // Hack for iOS when exit from Fullscreen mode
        if (is_iOS) {
            window.scrollTo(0, 0);
        }

        var app_container = document.getElementById('app-container');
        var game_canvas = document.getElementById('canvas');
        var innerWidth = window.innerWidth;
        var innerHeight = window.innerHeight - buttonHeight;
        var dpi = Module["getPixelRatio"]({{display.high_dpi}});
        var fullscreen = !!(document.fullscreenElement || document.fullScreen || document.mozFullScreen ||
                            document.webkitIsFullScreen || document.msIsFullScreen);
        if (innerWidth <= 0 || innerHeight <= 0) {
            return;
        }
        if (CUSTOM_PARAMETERS.resize_window_prev_inner_width == innerWidth &&
            CUSTOM_PARAMETERS.resize_window_prev_inner_height == innerHeight &&
            CUSTOM_PARAMETERS.resize_window_prev_pixel_ratio == dpi &&
            CUSTOM_PARAMETERS.resize_window_prev_fullscreen == fullscreen)
        {
            return;
        }
        CUSTOM_PARAMETERS.resize_window_prev_inner_width = innerWidth;
        CUSTOM_PARAMETERS.resize_window_prev_inner_height = innerHeight;
        CUSTOM_PARAMETERS.resize_window_prev_pixel_ratio = dpi;
        CUSTOM_PARAMETERS.resize_window_prev_fullscreen = fullscreen;
        var width = {{display.width}};
        var height = {{display.height}};
        var targetRatio = width / height;
        var actualRatio = innerWidth / innerHeight;
    {{#DEFOLD_SCALE_MODE_IS_DOWNSCALE_FIT}}
        //Downscale fit
        if (innerWidth < width || innerHeight < height) {
            if (actualRatio > targetRatio) {
                width = innerHeight * targetRatio;
                height = innerHeight;
                app_container.style.marginLeft = ((innerWidth - width) / 2) + "px";
                app_container.style.marginTop = "0px";
            }
            else {
                width = innerWidth;
                height = innerWidth / targetRatio;
                app_container.style.marginLeft = "0px";
                app_container.style.marginTop = ((innerHeight - height) / 2) + "px";
            }
        }
        else {
            app_container.style.marginLeft = ((innerWidth - width) / 2) + "px";
            app_container.style.marginTop = ((innerHeight - height) / 2) + "px";
        }
    {{/DEFOLD_SCALE_MODE_IS_DOWNSCALE_FIT}}
    {{#DEFOLD_SCALE_MODE_IS_STRETCH}}
        //Stretch
        width = innerWidth;
        height = innerHeight;
    {{/DEFOLD_SCALE_MODE_IS_STRETCH}}
    {{#DEFOLD_SCALE_MODE_IS_FIT}}
        //Fit
        if (actualRatio > targetRatio) {
            width = innerHeight * targetRatio;
            height = innerHeight;
            app_container.style.marginLeft = ((innerWidth - width) / 2) + "px";
            app_container.style.marginTop = "0px";
        }
        else {
            width = innerWidth;
            height = innerWidth / targetRatio;
            app_container.style.marginLeft = "0px";
            app_container.style.marginTop = ((innerHeight - height) / 2) + "px";
        }
    {{/DEFOLD_SCALE_MODE_IS_FIT}}
    {{#DEFOLD_SCALE_MODE_IS_NO_SCALE}}
        //No scale
        var margin_left = ((innerWidth - width) / 2);
        margin_left = margin_left > 0 ? margin_left : 0;
        var margin_top = ((innerHeight - height) / 2);
        margin_top = margin_top > 0 ? margin_top : 0;
        app_container.style.marginLeft = margin_left + "px";
        app_container.style.marginTop = margin_top + "px";
    {{/DEFOLD_SCALE_MODE_IS_NO_SCALE}}
        app_container.style.width = width + "px";
        app_container.style.height = height + buttonHeight + "px";
        // Fullscreen uses the whole viewport, matching the browser backend.
        var canvasWidth = Math.max(1, Math.floor((fullscreen ? window.innerWidth : width) * dpi));
        var canvasHeight = Math.max(1, Math.floor((fullscreen ? window.innerHeight : height) * dpi));
        if (game_canvas.width != canvasWidth) game_canvas.width = canvasWidth;
        if (game_canvas.height != canvasHeight) game_canvas.height = canvasHeight;
    }
};

// A scope owns cancellable work. Waiting on hashing or instantiation also settles
// immediately on cancellation; their eventual results can no longer advance startup.
function createLoaderScope(parent) {
    var callbacks = new Set();
    var releaseParent = function() {};
    var scope = {
        error: null,
        check: function() { if (this.error) throw this.error; },
        own: function(cancel) {
            if (this.error) cancel(this.error);
            else callbacks.add(cancel);
            return function() { callbacks.delete(cancel); };
        },
        cancel: function(error) {
            if (this.error) return;
            this.error = error || Object.assign(new Error("Loading cancelled"), { name: "AbortError" });
            releaseParent();
            callbacks.forEach(function(cancel) {
                try { cancel(scope.error); }
                catch (e) { console.warn("Failed to release loader resource: " + e); }
            });
            callbacks.clear();
        },
        wait: function(promise, cancel) {
            return new Promise(function(resolve, reject) {
                var release = scope.own(function(error) {
                    try { if (cancel) cancel(); }
                    finally { reject(error); }
                });
                Promise.resolve(promise).then(function(value) {
                    release();
                    if (scope.error) reject(scope.error);
                    else resolve(value);
                }, function(error) {
                    release();
                    reject(error);
                });
            });
        },
        delay: function(milliseconds) {
            this.check();
            var timer;
            return this.wait(new Promise(function(resolve) {
                timer = setTimeout(resolve, milliseconds);
            }), function() { clearTimeout(timer); });
        }
    };
    if (parent) releaseParent = parent.own(function(error) { scope.cancel(error); });
    return scope;
}

// Share request slots across archive files. Queued work is cancellable, and a
// transport retry releases its slot while waiting so other files can advance.
function createLoaderQueue(limit) {
    var active = 0;
    const pending = [];
    function pump() {
        while (active < limit && pending.length) {
            const task = pending.shift();
            task.release();
            ++active;
            task.resolve();
        }
    }
    return {
        run: async function(scope, work) {
            scope.check();
            await new Promise(function(resolve, reject) {
                const task = { resolve: resolve };
                task.release = scope.own(function(error) {
                    pending.splice(pending.indexOf(task), 1);
                    reject(error);
                });
                pending.push(task);
                pump();
            });
            try {
                scope.check();
                return await work();
            } finally {
                --active;
                pump();
            }
        }
    };
}

// file downloader
// wraps XMLHttpRequest and adds retry support and progress updates when the
// content is gzipped (gzipped content doesn't report a computable content length
// on Google Chrome)
var FileLoader = {
    options: {
        retryCount: 4,
        retryInterval: 1000,
    },
    addCacheBuster: function(url, attempt) {
        return url + (url.indexOf("?") === -1 ? "?" : "&") + "dmretry=" + attempt;
    },
    {{!
        Nothing is ever appended to what a url function returns: appending to a signed url
        whose signature covers the query string turns every retry into a 403.
    }}
    resolveUrl: function(url, attempt) {
        if (typeof url === "function") {
            return url(attempt);
        }
        return attempt > 0 ? FileLoader.addCacheBuster(url, attempt) : url;
    },
    notifyProgress: function(callback, delta, xhr, event, previous) {
        try {
            if (callback) callback(delta, xhr, event, previous);
        } catch (error) {
            console.error(error);
        }
    },
    // One XHR attempt. Unexpected transport failures are retryable at this layer.
    requestOnce: function(url, options, scope) {
        scope.check();
        const xhr = new XMLHttpRequest();
        var active = true;
        var loaded = 0;
        function abort() {
            // Scope cancellation already settles the promise and must not retry.
            if (active) { active = false; xhr.abort(); }
        }
        const promise = new Promise(function(resolve, reject) {
            xhr.open(options.method || "GET", url, true);
            xhr.responseType = options.responseType;
            xhr.onprogress = function(event) {
                if (!active) return;
                const previous = loaded;
                loaded = event.loaded;
                FileLoader.notifyProgress(options.progress, loaded - previous, xhr, event, previous);
            };
            xhr.onerror = xhr.ontimeout = xhr.onabort = function(event) {
                if (!active) return;
                active = false;
                reject(Object.assign(new Error(String(event)), { retryable: true, xhr: xhr, event: event }));
            };
            xhr.onload = function(event) {
                if (!active) return;
                active = false;
                resolve({ response: xhr.response, url: url, xhr: xhr, event: event });
            };
            xhr.send(null);
        });
        return scope.wait(promise, abort).finally(function() {
            abort();
            xhr.onload = xhr.onerror = xhr.ontimeout = xhr.onabort = xhr.onprogress = null;
        });
    },

    // Transport retries are a loop around a single request. Response validation
    // shares this budget; archive verification has its own whole-file budget.
    fetch: async function(url, options, scope) {
        for (var attempt = options.attempt || 0; ; ++attempt) {
            scope.check();
            const resolvedUrl = FileLoader.resolveUrl(url, attempt);
            var loaded = 0;
            try {
                const request = () => FileLoader.requestOnce(resolvedUrl, {
                    method: options.method,
                    responseType: options.responseType,
                    progress: function(delta, xhr, event, previous) {
                        loaded = event.loaded;
                        if (options.progress) options.progress(delta, xhr, event, previous);
                    }
                }, scope);
                const result = await (options.queue ? options.queue.run(scope, request) : request());
                scope.check();
                // Callback exceptions are terminal; only an explicit rejection retries.
                const rejected = options.accept && options.accept(result, attempt);
                try {
                    if (rejected) throw new Error(rejected);
                    if (!options.accept) {
                        if (result.xhr.status !== 200) throw new Error("status " + result.xhr.status);
                        if (options.responseType === "json" && typeof result.response === "string") {
                            result.response = JSON.parse(result.response);
                        }
                        const error = options.verify && options.verify(result.response);
                        if (error) throw new Error(error);
                    }
                } catch (error) {
                    throw Object.assign(error, { retryable: true, xhr: result.xhr, event: result.event });
                }
                scope.check();
                return result;
            } catch (error) {
                scope.check();
                if (!error.retryable || attempt >= FileLoader.options.retryCount) {
                    throw Object.assign(new Error("Error loading '" + resolvedUrl + "' (" + (error.message || String(error)) + ")"),
                        { download: true, xhr: error.xhr, event: error.event });
                }
                if (options.retry) options.retry(loaded, attempt, error);
                else FileLoader.notifyProgress(options.progress, -loaded);
                await scope.delay(FileLoader.options.retryInterval);
            }
        }
    },

    // Callback adapters for custom HTML templates. Core loading uses fetch().
    request: function(url, method, responseType, currentAttempt) {
        if (typeof method === "undefined") throw TypeError("No method specified");
        if (typeof responseType === "undefined") throw TypeError("No responseType specified");
        const scope = createLoaderScope();
        var attempt = currentAttempt || 0;
        var retryRequested = false;
        const request = {
            cancel: function() { scope.cancel(); },
            retry: function() {
                retryRequested = attempt < FileLoader.options.retryCount;
                return retryRequested;
            },
            send: function() {
                if (request.promise || scope.error) return;
                request.promise = FileLoader.fetch(function(index) {
                    request.url = FileLoader.resolveUrl(url, index);
                    return request.url;
                }, {
                    method: method, responseType: responseType, attempt: attempt,
                    progress: function(delta, xhr, event, previous) {
                        if (request.onprogress) request.onprogress(xhr, event, previous);
                    },
                    accept: function(result, index) {
                        attempt = index;
                        retryRequested = false;
                        if (request.onload) request.onload(result.xhr, result.event);
                        return retryRequested ? "Response rejected" : undefined;
                    },
                    retry: function(loaded, index, error) {
                        if (request.onretry) request.onretry(error.xhr, error.event, loaded, index);
                    }
                }, scope).catch(function(error) {
                    if (!scope.error && request.onerror) request.onerror(error.xhr, error.event || error);
                });
            }
        };
        return request;
    },
    size: function(url, callback) {
        const scope = createLoaderScope();
        FileLoader.fetch(url, { method: "HEAD", responseType: "text", accept: function() {} }, scope).then(
            result => callback(result.xhr.status === 200 ? result.xhr.getResponseHeader("content-length") : undefined),
            () => callback(undefined));
    },
    load: function(url, responseType, onprogress, onerror, onload, onretry, onverify) {
        const scope = createLoaderScope();
        const request = { cancel: function() { scope.cancel(); } };
        request.promise = FileLoader.fetch(function(attempt) {
            request.url = FileLoader.resolveUrl(url, attempt);
            return request.url;
        }, { responseType: responseType, progress: onprogress, retry: onretry, verify: onverify }, scope).then(
            result => { if (!scope.error) onload(result.response, result.url); },
            error => { if (!scope.error) onerror(error.message); });
        return request;
    },

    verifyHash: async function(data, expected) {
        if (!expected) return;
        const digest = await window.crypto.subtle.digest("SHA-1", data);
        const actual = Array.from(new Uint8Array(digest)).map(b => b.toString(16).padStart(2, '0')).join('');
        if (actual !== expected) throw new Error(`Unexpected hash ${actual} wanted ${expected}`);
    }
};


var EngineLoader = {
    arc_sha1: "{{DEFOLD_ARCHIVE_SHA1}}",
    wasm_sha1: "{{DEFOLD_WASM_SHA1}}",
    wasm_size: {{ DEFOLD_WASM_SIZE }},
    wasmjs_sha1: "{{DEFOLD_WASMJS_SHA1}}",
    wasmjs_size: {{ DEFOLD_WASMJS_SIZE }},
    wasm_pthread_sha1: "{{DEFOLD_WASM_PTHREAD_SHA1}}",
    wasm_pthread_size: {{ DEFOLD_WASM_PTHREAD_SIZE }},
    wasmjs_pthread_sha1: "{{DEFOLD_WASMJS_PTHREAD_SHA1}}",
    wasmjs_pthread_size: {{ DEFOLD_WASMJS_PTHREAD_SIZE }},

    stream_wasm: "{{html5.wasm_streaming}}" === "true",

    // Capture the variant once so every request and verification uses the same metadata.
    selectEngine: function(exeName) {
        const pthread = Module['isWASMPthreadSupported'];
        return {
            script: {
                url: exeName + (pthread ? "_pthread_wasm.js" : "_wasm.js"),
                size: pthread ? this.wasmjs_pthread_size : this.wasmjs_size,
                sha1: pthread ? this.wasmjs_pthread_sha1 : this.wasmjs_sha1
            },
            wasm: {
                url: exeName + (pthread ? "_pthread.wasm" : ".wasm"),
                size: pthread ? this.wasm_pthread_size : this.wasm_size,
                sha1: pthread ? this.wasm_pthread_sha1 : this.wasm_sha1
            },
            streaming: this.stream_wasm
        };
    },

    // Download and verify either engine asset before its bytes can be executed.
    fetchAndVerifyAsync: async function(asset, kind, scope) {
        const result = await FileLoader.fetch(asset.url, {
            responseType: "arraybuffer",
            progress: delta => ProgressUpdater.updateCurrent(delta),
            verify: function(response) {
                {{#html5.verify_downloaded_file_size}}
                if (response.byteLength != asset.size) {
                    return "Unexpected " + kind + " size: " + response.byteLength + ", expected: " + asset.size;
                }
                {{/html5.verify_downloaded_file_size}}
            }
        }, scope);
        scope.check();
        await scope.wait(FileLoader.verifyHash(result.response, asset.sha1));
        scope.check();
        return result;
    },

    // Each engine operation resolves only after its work completes. Its caller
    // owns failure reporting, so a failed verification cannot continue startup.
    loadAndInstantiateWasmAsync: async function(asset, imports, scope) {
        scope = scope || createLoaderScope();
        const result = await EngineLoader.fetchAndVerifyAsync(asset, "wasm", scope);
        // The download can finish before Emscripten supplies the imports.
        const importObject = await scope.wait(imports);
        scope.check();
        const output = await scope.wait(WebAssembly.instantiate(result.response, importObject));
        scope.check();
        return output;
    },

    streamAndInstantiateWasmAsync: async function(asset, imports, scope) {
        scope = scope || createLoaderScope();
        scope.check();
        var canCount = typeof TransformStream === "function" && ReadableStream.prototype.pipeThrough;
        // Hash verification requires the bytes before instantiation.
        if (asset.sha1{{#html5.verify_downloaded_file_size}} || !canCount{{/html5.verify_downloaded_file_size}}) {
            return EngineLoader.loadAndInstantiateWasmAsync(asset, imports, scope);
        }
        var streamScope = createLoaderScope(scope);
        var controller = new AbortController();
        streamScope.own(function() { controller.abort(); });
        var streamedSize = 0;
        async function fetchWithProgress() {
            const response = await fetch(asset.url, { signal: controller.signal });
            streamScope.check();
            if (!response.ok || !canCount) return response;
            const stream = new TransformStream({
                transform: function(chunk, target) {
                    streamScope.check();
                    streamedSize += chunk.byteLength;
                    ProgressUpdater.updateCurrent(chunk.byteLength);
                    streamScope.check();
                    target.enqueue(chunk);
                }
            });
            return new Response(response.body.pipeThrough(stream), response);
        }
        var output;
        try {
            // Start fetching now, and keep the response streaming when imports arrive.
            const ready = await streamScope.wait(Promise.all([fetchWithProgress(), imports]));
            streamScope.check();
            output = await streamScope.wait(WebAssembly.instantiateStreaming(ready[0], ready[1]));
            scope.check();
            {{#html5.verify_downloaded_file_size}}
            if (streamedSize != asset.size) {
                throw new Error("Unexpected wasm size: " + streamedSize + ", expected: " + asset.size);
            }
            {{/html5.verify_downloaded_file_size}}
        } catch (error) {
            scope.check();
            console.log('wasm streaming instantiation failed, falling back to XHR: ' + error);
            ProgressUpdater.updateCurrent(-streamedSize);
            // Return without awaiting so finally cancels the stream while the fallback downloads.
            return EngineLoader.loadAndInstantiateWasmAsync(asset, imports, scope);
        } finally {
            streamScope.cancel();
        }
        ProgressUpdater.updateCurrent(asset.instantiateProgress || 0);
        scope.check();
        return output;
    },

    loadWasmAsync: async function(engine, session) {
        const scope = session.scope;
        const importsReady = createLoaderSignal();
        const instantiate = engine.streaming && typeof WebAssembly.instantiateStreaming === "function"
            ? EngineLoader.streamAndInstantiateWasmAsync : EngineLoader.loadAndInstantiateWasmAsync;
        const instantiation = instantiate(engine.wasm, importsReady.promise, scope);
        Module.instantiateWasm = function(imports, successCallback) {
            (async function() {
                scope.check();
                if (typeof CUSTOM_PARAMETERS["update_imports"] === "function") {
                    CUSTOM_PARAMETERS["update_imports"](imports);
                }
                importsReady.resolve(imports);
                const output = await instantiation;
                scope.check();
                Module.instance = output.instance;
                successCallback(output.instance, output.module);
            })().catch(error => scope.cancel(error));
            return {}; // Emscripten expects exports to arrive through successCallback.
        };
        const onRuntimeInitialized = Module.onRuntimeInitialized;
        const initialize = function() {
            if (!scope.error && onRuntimeInitialized) onRuntimeInitialized.apply(this, arguments);
        };
        Module.onRuntimeInitialized = initialize;
        scope.own(function() {
            if (Module.onRuntimeInitialized === initialize) Module.onRuntimeInitialized = onRuntimeInitialized;
        });
        await scope.wait(Promise.all([
            instantiation,
            EngineLoader.loadAndRunScriptAsync(engine.script, scope),
            session.runtime.promise
        ]));
    },

    loadAndRunScriptAsync: async function(asset, scope) {
        scope = scope || createLoaderScope();
        const result = await EngineLoader.fetchAndVerifyAsync(asset, "JS", scope);
        scope.check();
        // Reuse the URL that passed verification, including any retry cache buster.
        Module["mainScriptUrlOrBlob"] = result.url;
        const script = document.createElement('script');
        script.src = result.url;
        // The DOM resolves relative URLs, matching the filename in window error events.
        asset.resolvedUrl = script.src;
        script.type = "text/javascript";
        try {
            await scope.wait(new Promise(function(resolve, reject) {
                script.onload = resolve;
                script.onerror = function() { reject(new Error("Error executing '" + result.url + "'")); };
                document.body.appendChild(script);
            }), function() { script.remove(); });
        } finally {
            script.onload = script.onerror = null;
        }
    },

    setupResizeCallback: function() {
        var callback = CUSTOM_PARAMETERS["resize_window_callback"];
        if (typeof callback !== "function") return;

        callback();
        window.addEventListener('resize', callback, false);
        window.addEventListener('orientationchange', callback, false);
        window.addEventListener('focus', callback, false);
        for (var event of ['fullscreenchange', 'mozfullscreenchange', 'webkitfullscreenchange', 'MSFullscreenChange']) {
            document.addEventListener(event, callback, false);
        }

        // Moving between displays can change DPR without changing the CSS viewport.
        // Watch the current resolution again after every change.
        var mediaQuery;
        var watchPixelRatio = function() {
            if (mediaQuery) {
                if (mediaQuery.removeEventListener) mediaQuery.removeEventListener('change', onPixelRatioChange);
                else mediaQuery.removeListener(onPixelRatioChange);
            }
            mediaQuery = window.matchMedia('(resolution: ' + (window.devicePixelRatio || 1) + 'dppx)');
            if (mediaQuery.addEventListener) mediaQuery.addEventListener('change', onPixelRatioChange);
            else mediaQuery.addListener(onPixelRatioChange);
        };
        var onPixelRatioChange = function() {
            watchPixelRatio();
            callback();
        };
        watchPixelRatio();
    },

    // Public entry point used by built-in and custom HTML templates.
    load: function(appCanvasId, exeName) {
        if (typeof CUSTOM_PARAMETERS["update_progress"] === "function") {
            ProgressUpdater.addListener(CUSTOM_PARAMETERS["update_progress"]);
        }

        ProgressView.addProgress(Module.setupCanvas(appCanvasId));
        CUSTOM_PARAMETERS['exe_name'] = exeName;

        FileLoader.options.retryCount = CUSTOM_PARAMETERS["retry_count"];
        FileLoader.options.retryInterval = CUSTOM_PARAMETERS["retry_time"] * 1000;
        GameArchiveLoader.setFileLocationFilter(CUSTOM_PARAMETERS["archive_location_filter"]);
        const ready = GameStartup.start('/archive_files.json', exeName);

        // move resize callback setup here to make possible to override callback
        // from outside of dmloader.js
        EngineLoader.setupResizeCallback();
        return ready;
    }
};


function createLoaderSignal() {
    var resolve;
    const promise = new Promise(function(done) { resolve = done; });
    return { promise: promise, resolve: resolve };
}

// The only owner of startup ordering and failure reporting. Emscripten hooks
// resolve milestones; archive listeners are notifications, not control flow.
var GameStartup = {
    _session: null,

    begin: function() {
        this.cancel();
        const session = {
            scope: createLoaderScope(), state: "loading", pendingFiles: [], archive: null,
            runtime: createLoaderSignal(), filesystem: createLoaderSignal(), storage: createLoaderSignal()
        };
        if (Module._filesystemReady) session.filesystem.resolve();
        if (Module._syncInitial || !Module.persistentStorage) session.storage.resolve();
        this._session = session;
        Module._preLoadDone = false;
        Module._filesToPreload = session.pendingFiles;
        GameArchiveLoader.isCompleted = false;
        return session;
    },
    cancel: function() {
        const session = this._session;
        this._session = null;
        if (session) {
            session.scope.cancel();
            session.pendingFiles.length = 0;
        }
        GameArchiveLoader._files = [];
    },
    fail: function(session, error) {
        if (this._session !== session || session.state === "failed") return;
        session.state = "failed";
        session.scope.cancel(error);
        session.pendingFiles.length = 0;
        // Startup promises are handled even when a template ignores them. Keep
        // terminal failures visible regardless of the optional error callbacks.
        console.error(error);
        if (error.archive) {
            const message = error.download ? error.message : String(error);
            GameArchiveLoader.notifyListeners(GameArchiveLoader._onFileDownloadErrorListeners, message);
            const callback = CUSTOM_PARAMETERS["can_not_download_file_callback"] || CUSTOM_PARAMETERS["start_error"];
            if (typeof callback === "function") callback(message);
        } else if (typeof CUSTOM_PARAMETERS["start_error"] === "function") {
            CUSTOM_PARAMETERS["start_error"](error);
        }
    },
    track: function(session, promise) {
        const completion = session.scope.wait(promise).catch(error => {
            this.fail(session, error);
            throw error;
        });
        // Callback callers may ignore completion; awaiters still receive rejection.
        completion.catch(function() {});
        return completion;
    },
    start: function(descriptionUrl, exeName) {
        const session = this.begin();
        session.engine = EngineLoader.selectEngine(exeName || CUSTOM_PARAMETERS['exe_name']);
        session.ready = this.track(session, this.run(session, descriptionUrl));
        return session.ready;
    },
    run: async function(session, descriptionUrl) {
        const scope = session.scope;
        if (!Module['isWASMSupported']) throw new Error("WebAssembly is not supported in this browser.");
        const engine = session.engine;
        const engineSize = engine.wasm.size + engine.script.size;
        engine.wasm.instantiateProgress = engineSize * 0.1;
        ProgressUpdater.resetCurrent();
        // Count early engine bytes, but wait for the manifest before reporting a percentage.
        ProgressUpdater.setupTotal(0);
        const archive = GameArchiveLoader.loadDescription(descriptionUrl, scope).then(manifest => {
            scope.check();
            ProgressUpdater.setupTotal(manifest.total_size + engineSize + engine.wasm.instantiateProgress);
            ProgressUpdater.setCurrent(ProgressUpdater.current);
            scope.check();
            GameArchiveLoader._files = manifest.content;
            return GameArchiveLoader.downloadContent(manifest.content, session);
        });

        await scope.wait(Promise.all([
            EngineLoader.loadWasmAsync(engine, session),
            archive,
            session.storage.promise
        ]));
        scope.check();
        Module.preloadAll();
        ProgressUpdater.complete();
        scope.check();
        Module._callMain();
        scope.check();
        session.state = "started";
    }
};


/* ********************************************************************* */
/* Load and combine game archive data that is split into archives        */
/* ********************************************************************* */

// Storage owns buffers and streams; orchestration only writes, verifies, and
// finishes a destination. Disposing an unfinished DMFS file removes partial data.
var ArchiveStorage = {
    readCached: async function(file, scope) {
        if (!Module['isDMFSSupported']) return false;
        const path = `${DMSYS.GetUserPersistentDataRoot()}/${file.name}`;
        var stream = null;
        var valid = false;
        function close() {
            const current = stream;
            stream = null;
            if (current) FS.close(current);
        }
        const release = scope.own(close);
        try {
            const stat = FS.stat(path);
            {{#html5.verify_downloaded_file_size}}
            if (stat.size != file.size) return false;
            {{/html5.verify_downloaded_file_size}}
            {{^html5.verify_downloaded_file_size}}
            if (!file.sha1) return false;
            {{/html5.verify_downloaded_file_size}}
            if (!file.sha1) return true;
            stream = FS.open(path, "r");
            const data = FS.mmap(stream, stat.size, 0, 0x01, 0x01); // PROT_READ, MAP_SHARED
            if (data) {
                await scope.wait(FileLoader.verifyHash(data, file.sha1));
                valid = true;
            }
        } catch (error) {
            scope.check(); // Cancellation is not a cache miss.
        } finally {
            release();
            try { close(); }
            catch (error) { valid = false; }
        }
        scope.check();
        return valid;
    },

    create: function(file) {
        if (Module['isDMFSSupported']) {
            const path = `${DMSYS.GetUserPersistentDataRoot()}/${file.name}`;
            var stream = FS.open(path, "w+");
            var committed = false;
            function close() {
                var current = stream;
                stream = null;
                if (current) FS.close(current);
            }
            return {
                write: function(index, data) { FS.write(stream, data, 0, data.length, file.pieces[index].offset); },
                bytes: function(size) { return FS.mmap(stream, size, 0, 0x01, 0x01); },
                finish: function() { close(); committed = true; },
                dispose: function() {
                    try { close(); }
                    finally { if (!committed) FS.unlink(path); }
                }
            };
        }
        var buffer;
        return {
            write: function(index, data) {
                if (file.pieces.length === 1) {
                    buffer = data;
                    return;
                }
                if (!buffer) buffer = new Uint8Array(file.size);
                const piece = file.pieces[index];
                const end = piece.offset + data.length;
                {{^html5.verify_downloaded_file_size}}
                if (end > buffer.length) {
                    const grown = new Uint8Array(end);
                    grown.set(buffer);
                    buffer = grown;
                }
                {{/html5.verify_downloaded_file_size}}
                // Let verification reject oversized responses after all pieces settle.
                if (piece.offset >= 0 && end <= buffer.length) buffer.set(data, piece.offset);
            },
            bytes: function() { return buffer; },
            finish: function() { const result = buffer; buffer = undefined; return result; },
            dispose: function() { buffer = undefined; }
        };
    }
};

var GameArchiveLoader = {
    _files: [],
    isCompleted: false,
    MAX_CONCURRENT_XHR: 6, // Shared across all archive files, including transport retries.
    MAX_VERIFICATION_RETRIES: 2,
    _onFileLoadedListeners: [],
    _onArchiveLoadedListeners: [],
    _onFileDownloadErrorListeners: [],

    _archiveLocationFilter: function(path, attempt) {
        var url = "split" + path;
        return attempt ? FileLoader.addCacheBuster(url, attempt) : url;
    },
    archiveLocationProvider: function(path, verificationAttempt) {
        var base = (verificationAttempt || 0) * (FileLoader.options.retryCount + 1);
        return attempt => this._archiveLocationFilter(path, base + attempt);
    },
    setFileLocationFilter: function(filter) {
        if (typeof filter !== 'function') throw TypeError("Invalid filter");
        this._archiveLocationFilter = filter;
    },

    // Compatibility entry points; startup itself lives in GameStartup.run().
    loadArchiveDescription: function(url) { return GameStartup.start(url); },
    cleanUp: function() { GameStartup.cancel(); },
    notifyFileDownloadError: function(error) {
        const session = GameStartup._session;
        if (session) GameStartup.fail(session, Object.assign(new Error(error), { archive: true, download: true }));
    },

    addListener: function(list, callback) {
        if (typeof callback !== 'function') throw TypeError("Invalid callback registration");
        list.push(callback);
    },
    notifyListeners: function(list, data, session) {
        for (const callback of list) {
            if (session) session.scope.check();
            callback(data);
        }
    },
    addFileDownloadErrorListener: function(callback) { this.addListener(this._onFileDownloadErrorListeners, callback); },
    addFileLoadedListener: function(callback) { this.addListener(this._onFileLoadedListeners, callback); },
    addArchiveLoadedListener: function(callback) { this.addListener(this._onArchiveLoadedListeners, callback); },

    loadDescription: async function(url, scope) {
        try {
            const result = await FileLoader.fetch(this.archiveLocationProvider(url, 0), { responseType: "text" }, scope);
            scope.check();
            return await this.parseDescription(result.response, scope);
        } catch (error) {
            scope.check();
            throw Object.assign(error, { archive: true });
        }
    },
    parseDescription: async function(text, scope) {
        const json = JSON.parse(text);
        if (!Array.isArray(json.content)) throw new Error("Invalid archive description: missing content");
        json.content.forEach(function(file, index) {
            if (!file || !Array.isArray(file.pieces) || file.pieces.length === 0) {
                throw new Error("Invalid archive file at index " + index);
            }
        });
        if (EngineLoader.arc_sha1) {
            await scope.wait(FileLoader.verifyHash(new TextEncoder().encode(text), EngineLoader.arc_sha1));
        }
        scope.check();
        return json;
    },

    downloadContent: function(files, session) {
        const standalone = !session;
        files = files || this._files;
        session = session || GameStartup._session || GameStartup.begin();
        if (!session.archive) {
            const download = Promise.resolve().then(() => this.downloadFiles(files, session)).catch(function(error) {
                session.scope.check();
                throw Object.assign(error, { archive: true });
            });
            session.archive = standalone ? GameStartup.track(session, download) : download;
        }
        return session.archive;
    },
    downloadFiles: async function(files, session) {
        if (Module['isDMFSSupported'] && files.length) await session.scope.wait(session.filesystem.promise);
        session.scope.check();
        const limit = this.MAX_CONCURRENT_XHR || 6;
        const queue = createLoaderQueue(limit);
        // DMFS mappings may allocate a whole file. Share one verification slot
        // between cache reads and downloaded files without limiting their XHRs.
        const verificationQueue = Module['isDMFSSupported'] ? createLoaderQueue(1) : null;
        const completed = [];
        var nextFile = 0;
        await Promise.all(files.map(async (file, index) => {
            completed[index] = await this.downloadFile(file, session.scope, queue, limit, verificationQueue);
            session.scope.check();
            // Downloads may finish out of order; keep public notifications in manifest order.
            while (completed[nextFile]) {
                const result = completed[nextFile];
                completed[nextFile++] = undefined;
                Module.onArchiveFileLoaded(result);
                this.notifyListeners(this._onFileLoadedListeners, result, session);
                session.scope.check();
            }
        }));
        this.isCompleted = true;
        this._files = [];
        Module.onArchiveLoaded();
        this.notifyListeners(this._onArchiveLoadedListeners, undefined, session);
    },

    downloadFile: async function(file, scope, queue, limit, verificationQueue) {
        scope.check();
        const readCached = () => ArchiveStorage.readCached(file, scope);
        if (await (verificationQueue ? verificationQueue.run(scope, readCached) : readCached())) {
            scope.check();
            return { name: file.name };
        }
        for (var retry = 0; ; ++retry) {
            scope.check();
            const attemptScope = createLoaderScope(scope);
            const attempt = { file: file, scope: attemptScope, pieceLengths: [], receivedBytes: 0 };
            try {
                attempt.storage = ArchiveStorage.create(file);
                attemptScope.own(() => attempt.storage.dispose());
                await this.downloadPieces(attempt, retry, queue, limit);
                attemptScope.check();
                const verifyAndFinish = async () => {
                    try {
                        await attemptScope.wait(this.verifyFile(attempt));
                        attemptScope.check();
                        return { name: file.name, data: attempt.storage.finish() };
                    } finally {
                        // Close failed mappings before releasing the verification slot.
                        attemptScope.cancel();
                    }
                };
                try {
                    return await (verificationQueue ? verificationQueue.run(attemptScope, verifyAndFinish) : verifyAndFinish());
                } catch (error) {
                    scope.check();
                    if (retry >= this.MAX_VERIFICATION_RETRIES) throw error;
                    console.warn("Verification of " + file.name + " failed, downloading it again: " + error);
                }
            } finally {
                attemptScope.cancel();
            }
            // Transport retries already roll back their own progress. Only bytes
            // from completed pieces belong to this discarded verification attempt.
            ProgressUpdater.updateCurrent(-attempt.receivedBytes);
            await scope.delay(FileLoader.options.retryInterval);
        }
    },
    downloadPieces: async function(attempt, verificationAttempt, queue, limit) {
        const file = attempt.file;
        var nextPiece = 0;
        const worker = async () => {
            while (nextPiece < file.pieces.length) {
                attempt.scope.check();
                const index = nextPiece++;
                const result = await FileLoader.fetch(this.archiveLocationProvider('/' + file.pieces[index].name, verificationAttempt), {
                    responseType: "arraybuffer",
                    queue: queue,
                    progress: delta => ProgressUpdater.updateCurrent(delta)
                }, attempt.scope);
                attempt.scope.check();
                const data = new Uint8Array(result.response);
                attempt.storage.write(index, data);
                attempt.pieceLengths[index] = data.length;
                attempt.receivedBytes += data.length;
            }
        };
        await Promise.all(Array.from({ length: Math.min(file.pieces.length, limit) }, worker));
    },
    verifyFile: async function(attempt) {
        const file = attempt.file;
        {{#html5.verify_downloaded_file_size}}
        if (attempt.receivedBytes != file.size) {
            throw new Error("Unexpected data size: " + file.name + ", expected size: " + file.size + ", actual size: " + attempt.receivedBytes);
        }
        {{/html5.verify_downloaded_file_size}}
        for (var i = 1; i < file.pieces.length; ++i) {
            const previousEnd = file.pieces[i - 1].offset + attempt.pieceLengths[i - 1];
            if (previousEnd > file.pieces[i].offset) {
                throw new RangeError("Segment underflow in file: " + file.name + ", offset: " + previousEnd + ", start: " + file.pieces[i].offset);
            }
        }
        if (file.sha1) {
            const data = attempt.storage.bytes(attempt.receivedBytes);
            if (!data) throw new Error("Cannot read " + file.name + " for hash verification");
            await FileLoader.verifyHash(data, file.sha1);
        }
    }
};

/* ********************************************************************* */
/* Default splash and progress visualisation                             */
/* ********************************************************************* */

var ProgressView = {
    progress_id: "defold-progress",
    bar_id: "defold-progress-bar",

    addProgress : function (canvas) {
        /* Insert default progress bar below canvas */
        canvas.insertAdjacentHTML('afterend', '<div id="' + ProgressView.progress_id + '" class="canvas-app-progress"><div id="' + ProgressView.bar_id + '" class="canvas-app-progress-bar" style="transform: scaleX(0.0);"></div></div>');
        ProgressView.bar = document.getElementById(ProgressView.bar_id);
        ProgressView.progress = document.getElementById(ProgressView.progress_id);
    },

    updateProgress: function(percentage) {
        if (ProgressView.bar) {
            ProgressView.bar.style.transform = "scaleX(" + Math.min(percentage, 100) / 100 + ")";
        }
    },

    removeProgress: function () {
        if (ProgressView.progress.parentElement !== null) {
            ProgressView.progress.parentElement.removeChild(ProgressView.progress);

            // Remove any background/splash image that was set in runApp().
            // Workaround for Safari bug DEF-3061.
            Module.canvas.style.background = "none";
        }
    }
};

var ProgressUpdater = {
    current: 0,
    total: 1,

    listeners: [],

    addListener: function(callback) {
        if (typeof callback !== 'function') throw TypeError("Invalid callback registration");
        this.listeners.push(callback);
    },

    notifyListeners: function(percentage) {
        for (var i=0; i<this.listeners.length; ++i) {
            try {
                this.listeners[i](percentage);
            } catch (error) {
                console.error(error);
            }
        }
    },

    setupTotal: function (total) {
        this.total = total;
    },

    setCurrent: function (current) {
        this.current = current;
        var percentage = this.calculateProgress();
        try {
            ProgressView.updateProgress(percentage);
        } catch (error) {
            console.error(error);
        }
        this.notifyListeners(percentage);
    },

    updateCurrent: function (diff) {
        this.setCurrent(this.current + diff);
    },

    resetCurrent: function () {
        this.current = 0;
    },

    complete: function () {
        this.setCurrent(this.total);
    },

    calculateProgress: function () {
        return this.total ? this.current / this.total * 100 : 0;
    }
};

/* DEPRECATED!
* Use ProgressUpdater and ProgressView instead.
* Left for backward compatability.
*/
var Progress = {
    addListener: function(callback) {
        ProgressUpdater.addListener(callback);
    },

    notifyListeners: function(percentage) {
        // no-op
    },

    addProgress : function (canvas) {
        ProgressView.addProgress(canvas);
    },

    updateProgress: function(percentage) {
        // no-op
    },

    calculateProgress: function (from, to, current, total) {
        // no-op
    },

    removeProgress: function () {
        ProgressView.removeProgress();
    }
};

/* ********************************************************************* */
/* Module is Emscripten namespace                                        */
/* ********************************************************************* */

var Module = {
    engineVersion: "{{DEFOLD_ENGINE_VERSION}}",
    engineSdkSha1: "{{DEFOLD_SDK_SHA1}}",
    noInitialRun: true,

    // Shared with the browser backend so fullscreen and display scale use the same rendering ratio.
    getPixelRatio: function(highDPI) {
        if (!highDPI) return 1;
        var ratio = window.devicePixelRatio || 1;
        var minRatio = {{DEFOLD_MIN_PIXEL_RATIO}};
        var maxRatio = {{DEFOLD_MAX_PIXEL_RATIO}};
        if (minRatio > 0) ratio = Math.max(ratio, minRatio);
        if (maxRatio > 0) ratio = Math.min(ratio, maxRatio);
        return ratio;
    },

    _filesToPreload: [],
    _preLoadDone: false,
    _isMainCalled: false,
    _filesystemReady: false,

    // Persistent storage
    persistentStorage: true,
    _syncInProgress: false,
    _syncNeeded: false,
    _syncInitial: false,
    _syncMaxTries: 3,
    _syncTries: 0,

    arguments: [],

    print: function(text) { console.log(text); },
    printErr: function(text) { console.error(text); },

    setStatus: function(text) { console.log(text); },

    reportEngineError: function(error) {
        // A custom callback must not interrupt Emscripten's abort handling.
        try {
            const session = GameStartup._session;
            if (session) {
                GameStartup.fail(session, error);
            } else {
                console.error(error);
                if (typeof CUSTOM_PARAMETERS["start_error"] === "function") {
                    CUSTOM_PARAMETERS["start_error"](error);
                }
            }
        } catch (callbackError) {
            console.error(callbackError);
        }
    },

    onAbort: function(reason) {
        Module.reportEngineError(new Error("Engine aborted: " + reason));
    },

    isDMFSSupported: (function() {
        // DMFS is meant as a mount for FS to provide another way to acess resources, by default we just use IDBFS
        if (typeof DMFS === "undefined")
            return false;
        return true;
    })(),

    isWASMSupported: (function() {
        try {
            if (typeof WebAssembly === "object" && typeof WebAssembly.instantiate === "function") {
                const module = new WebAssembly.Module(Uint8Array.of(0x0, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00));
                if (module instanceof WebAssembly.Module)
                    return new WebAssembly.Instance(module) instanceof WebAssembly.Instance;
            }
        } catch (e) {
        }
        return false;
    })(),

    prepareErrorObject: function (err, url, line, column, errObj) {
        line = typeof line == "undefined" ? 0 : line;
        column = typeof column == "undefined" ? 0 : column;
        url = typeof url == "undefined" ? "" : url;
        var errorLine = url + ":" + line + ":" + column;

        var error = errObj || (typeof window.event != "undefined" ? window.event.error : "" ) || err || "Undefined Error";
        var message = "";
        var stack = "";

        if (typeof error == "object" && typeof error.stack != "undefined" && typeof error.message != "undefined") {
            stack = String(error.stack);
            message = String(error.message);
        } else {
            stack = String(error).split("\n");
            message = stack.shift();
            stack = stack.join("\n");
        }
        stack = stack || errorLine;

        var callLine = /at (\S+:\d*$)/.exec(message);
        if (callLine) {
            message = message.replace(/(at \S+:\d*$)/, "");
            stack = callLine[1] + "\n" + stack;
        }

        message = message.replace(/(abort\(.+\)) at .+/, "$1");
        stack = stack.replace(/\?{1}\S+(:\d+:\d+)/g, "$1");
        stack = stack.replace(/ *at (\S+)$/gm, "@$1");
        stack = stack.replace(/ *at (\S+)(?: \[as \S+\])? +\((.+)\)/g, "$1@$2");
        stack = stack.replace(/^((?:Object|Array)\.)/gm, "");
        stack = stack.split("\n");

        return { stack:stack, message:message };
    },

    {{#DEFOLD_HAS_WEBGPU}}
    probeWebGPUSupport: function(callback, scope) {
        Module._webgpuAdapterAvailable = false;
        var can_probe_adapter = false;

        try {
            if (typeof navigator !== "undefined" && navigator.gpu && typeof navigator.gpu.requestAdapter === "function") {
                var canvas = document.createElement("canvas");
                can_probe_adapter = !!canvas.getContext("webgpu");
            }
        } catch (error) {
            console.log("An error occurred while detecting WebGPU support: " + error);
        }

        if (!can_probe_adapter) {
            callback();
            return;
        }

        try {
            navigator.gpu.requestAdapter().then(function(adapter) {
                if (scope && scope.error) return;
                Module._webgpuAdapterAvailable = adapter !== null && typeof adapter !== "undefined";
                callback();
            }, function(error) {
                console.log("An error occurred while detecting WebGPU support: " + error);
                callback();
            });
        } catch (error) {
            console.log("An error occurred while detecting WebGPU support: " + error);
            callback();
        }
    },

    hasWebGPUSupport: function() {
        return Module._webgpuAdapterAvailable === true;
    },
    {{/DEFOLD_HAS_WEBGPU}}

    hasWebGLSupport: function() {
        var webgl_support = false;
        try {
            // create canvas to simply check is rendering context supported
            // real render context created by glfw
            var canvas = document.createElement("canvas");
            var gl = canvas.getContext("webgl") || canvas.getContext("experimental-webgl");
            if (gl && gl instanceof WebGLRenderingContext) {
                webgl_support = true;
            }
        } catch (error) {
            console.log("An error occurred while detecting WebGL support: " + error);
            webgl_support = false;
        }

        return webgl_support;
    },

    setupCanvas: function(appCanvasId) {
        appCanvasId = (typeof appCanvasId === 'undefined') ? 'canvas' : appCanvasId;
        Module.canvas = document.getElementById(appCanvasId);
        return Module.canvas;
    },


    /**
    * Module.runApp - Starts the application given a canvas element id
    **/
    runApp: function(appCanvasId, session) {
        session = session || GameStartup._session;
        window.addEventListener("error", (errorEvent) => {
            var errorObject = Module.prepareErrorObject(errorEvent.message, errorEvent.filename, errorEvent.lineno, errorEvent.colno, errorEvent.error);
            Module.ccall('JSWriteDump', 'null', ['string'], [JSON.stringify(errorObject.stack)]);
        });
        Module.setupCanvas(appCanvasId);

        Module.arguments = CUSTOM_PARAMETERS["engine_arguments"];

        var fullScreenContainer = CUSTOM_PARAMETERS["full_screen_container"];
        if (typeof fullScreenContainer === "string") {
            fullScreenContainer = document.querySelector(fullScreenContainer);
        }
        Module.fullScreenContainer = fullScreenContainer || Module.canvas;

        function startApp() {
            if (session && session.scope.error) return;
            if (Module.hasWebGLSupport(){{#DEFOLD_HAS_WEBGPU}} || Module.hasWebGPUSupport(){{/DEFOLD_HAS_WEBGPU}}) {
                Module.canvas.focus();

                Module.canvas.addEventListener("webglcontextlost", function(event) {
                    event.preventDefault();
                    dmRenderer.rendererContextEvent(dmRenderer.CONTEXT_LOST_EVENT);
                }, false);
                Module.canvas.addEventListener("webglcontextrestored", function(event) {
                    dmRenderer.rendererContextEvent(dmRenderer.CONTEXT_RESTORED_EVENT);
                }, false);
                // Add context menu hide-handler if requested
                if (CUSTOM_PARAMETERS["disable_context_menu"])
                {
                    Module.canvas.oncontextmenu = function(e) {
                        e.preventDefault();
                    };
                }
                if (session) session.runtime.resolve();
            } else {
                // "Unable to start game, WebGL not supported"
                ProgressUpdater.complete();
                Module.setStatus = function(text) {
                    if (text) Module.printErr('[missing WebGL] ' + text);
                };

                if (typeof CUSTOM_PARAMETERS["unsupported_webgl_callback"] === "function") {
                    CUSTOM_PARAMETERS["unsupported_webgl_callback"]();
                }
                if (session) GameStartup.fail(session, new Error("No supported graphics adapter"));
            }
        }
        {{#DEFOLD_HAS_WEBGPU}}
        // Adapter selection in the engine is synchronous. Resolve the WebGPU
        // adapter before marking the engine loaded so other callbacks cannot
        // start main() before WebGPUIsSupported() can use the cached result.
        Module.probeWebGPUSupport(startApp, session && session.scope);
        {{/DEFOLD_HAS_WEBGPU}}
        {{^DEFOLD_HAS_WEBGPU}}
        startApp();
        {{/DEFOLD_HAS_WEBGPU}}
    },

    onArchiveFileLoaded: function(file) {
        if (file.data) {
            Module._filesToPreload.push({path: file.name, data: file.data});
        }
    },

    // Notification hook retained for custom HTML templates.
    onArchiveLoaded: function() {},

    toggleFullscreen: function(element) {
        if (DefoldPlatform.isFullscreen) {
            DefoldPlatform.cancelFullScreen();
        } else {
            DefoldPlatform.requestFullScreen(element);
        }
    },

    preSync: function(done) {
        if (Module.persistentStorage != true) {
            done();
            return;
        }
        // Initial persistent sync before main is called
        FS.syncfs(true, function(err) {
            if (err) {
                Module._syncTries += 1;
                console.info(`Unable to synchronize mounted file systems (attempt ${Module._syncTries} of ${Module._syncMaxTries}): `, err);
                if (Module._syncMaxTries > Module._syncTries) {
                    Module.preSync(done);
                } else {
                    console.warn("Mounted system wasn't synchronized. Retry count was exceeded.");
                    Module._syncTries = 0;
                    Module._syncInitial = true;
                    done();
                }
            } else {
                Module._syncTries = 0;
                Module._syncInitial = true;
                if (done !== undefined) {
                    done();
                }
            }
        });
    },

    preloadAll: function() {
        if (Module._preLoadDone) {
            return;
        }
        Module._preLoadDone = true;
        for (var i = 0; i < Module._filesToPreload.length; ++i) {
            var item = Module._filesToPreload[i];
            // Let the filesystem own the downloaded buffer instead of copying it.
            FS.createPreloadedFile("", item.path, item.data, true, true, undefined, undefined, false, true);
        }
        Module._filesToPreload.length = 0;
    },

    // Tries to do a MEM->IDB sync
    // It will flag that another one is needed if there is already one sync running.
    persistentSync: function() {

        if (Module.persistentStorage != true) {
            return;
        }
        // Need to wait for the initial sync to finish since it
        // will call close on all its file streams which will trigger
        // new persistentSync for each.
        if (Module._syncInitial) {
            if (Module._syncInProgress) {
                Module._syncNeeded = true;
            } else {
                Module._startSyncFS();
            }
        }
    },

    preInit: [function() {
        const session = GameStartup._session;
        function storageReady() { if (session) session.storage.resolve(); }
        // Mount filesystem on preinit
        var dir = DMSYS.GetUserPersistentDataRoot();
        try {
            FS.mkdir(dir);
        }
        catch (error) {
            Module.persistentStorage = false;
            storageReady();
            return;
        }

        try {
            if (Module['isDMFSSupported']) {
                // In DMFS mode we will use that as our mountpoint and make sure that all
                // relative paths point into there.
                FS.mount(new DMFS(CUSTOM_PARAMETERS['exe_name']), {}, dir);
                FS.chdir(dir);
            } else {
                // If IndexedDB is supported we mount the persistent data root as IDBFS,
                // then try to do a IDB->MEM sync before we start the engine to get
                // previously saved data before boot.
                FS.mount(IDBFS, {}, dir);
            }
            // Patch FS.close so it will try to sync MEM->IDB
            var _close = FS.close;
            FS.close = function(stream) {
                var r = _close(stream);
                Module.persistentSync();
                return r;
            };
        }
        catch (error) {
            Module.persistentStorage = false;
            storageReady();
            return;
        }

        // Sync IDB->MEM before calling main()
        Module.preSync(storageReady);
    }],

    preRun: [],

    postRun: [function() {
        Module._filesystemReady = true;
        const session = GameStartup._session;
        if (session) session.filesystem.resolve();
    }],

    _callMain: function(_, _) {
        if (!Module._isMainCalled) {
            const session = GameStartup._session;
            Module._isMainCalled = true;
            ProgressView.removeProgress();
            if (typeof CUSTOM_PARAMETERS["start_success"] === "function") {
                CUSTOM_PARAMETERS["start_success"]();
            }
            if (session) session.scope.check();
            const status = Module.callMain(Module.arguments);
            // Emscripten catches ExitStatus internally and returns its code even
            // when the runtime is kept alive.
            if (typeof status === "number" && status !== 0) {
                throw new Error("Engine exited with code " + status);
            }
        } else {
            console.warn("Main was called several times!");
        }
    },
    // Wrap IDBFS syncfs call with logic to avoid multiple syncs
    // running at the same time.
    _startSyncFS: function() {
        if (Module._syncMaxTries > Module._syncTries) {
            Module._syncInProgress = true;
            FS.syncfs(false, function(err) {
                Module._syncInProgress = false;

                if (err) {
                    console.info(`Unable to synchronize mounted file systems (attempt ${Module._syncTries} of ${Module._syncMaxTries}): `, err);
                    Module._syncTries += 1;
                } else {
                    Module._syncTries = 0;
                }

                if (Module._syncNeeded) {
                    Module._syncNeeded = false;
                    Module._startSyncFS();
                }

            });
        } else {
            console.warn("Mounted system wasn't synchronized. Retry count was exceeded.");
            Module._syncTries = 0;
        }
    },
};

// common engine setup
Module['persistentStorage'] = (typeof window !== 'undefined') && !!(window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB);

Module['INITIAL_MEMORY'] = CUSTOM_PARAMETERS.custom_heap_size;

Module['onRuntimeInitialized'] = function() {
    Module.runApp("canvas");
};

Module["isWASMPthreadSupported"] = {{DEFOLD_HAS_WASM_PTHREAD_ENGINE}} 
    && ((typeof window === 'undefined') || window.isSecureContext && window.crossOriginIsolated)
    && typeof SharedArrayBuffer !== 'undefined';

Module["locateFile"] = function(path, scriptDirectory)
{
    // dmengine*.wasm is hardcoded in the built JS loader for WASM,
    // we need to replace it here with the correct project name.
    if (path == "dmengine.wasm" || path == "dmengine_release.wasm" || path == "dmengine_headless.wasm") {
        const session = GameStartup._session;
        const engine = session && session.engine || EngineLoader.selectEngine("{{exe-name}}");
        path = engine.wasm.url.substring(engine.wasm.url.lastIndexOf('/') + 1);
    }
    return scriptDirectory + path;
};

{{^DEFOLD_HAS_WASM_ENGINE}}
Module["isWASMSupported"] = false;
{{/DEFOLD_HAS_WASM_ENGINE}}

window.addEventListener("error", (errorEvent) => {
    const session = GameStartup._session;
    const engineScript = session && session.engine && session.engine.script;
    if (session && session.state !== "started" && engineScript && engineScript.resolvedUrl &&
        errorEvent.filename === engineScript.resolvedUrl) {
        GameStartup.fail(session, errorEvent);
    } else if ((!session || session.state !== "failed") && typeof CUSTOM_PARAMETERS["start_error"] === "function") {
        CUSTOM_PARAMETERS["start_error"](errorEvent);
    }
    Module.setStatus('Exception thrown, see JavaScript console');
    Module.setStatus = function(text) {
        if (text) Module.printErr('[post-exception status] ' + text);
    };
});
