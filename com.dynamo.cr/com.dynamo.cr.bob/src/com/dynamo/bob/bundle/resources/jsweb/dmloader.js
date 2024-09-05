/*
*     'archive_location_filter':
*         Filter function that will run for each archive path.
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
*         Function that is called when resize/orientationchanges/focus events happened
*/
var CUSTOM_PARAMETERS = {
    archive_location_filter: function( path ) {
        return ("{{DEFOLD_ARCHIVE_LOCATION_PREFIX}}" + path + "{{DEFOLD_ARCHIVE_LOCATION_SUFFIX}}");
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
    resize_window_callback: function() {
        var is_iOS = /iPad|iPhone|iPod/.test(navigator.userAgent) && !window.MSStream;
        var buttonHeight = 0;
        var prevInnerWidth = -1;
        var prevInnerHeight = -1;
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
        if (prevInnerWidth == innerWidth && prevInnerHeight == innerHeight)
        {
            return;
        }
        prevInnerWidth = innerWidth;
        prevInnerHeight = innerHeight;
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
        var dpi = 1;
    {{#display.high_dpi}}
        dpi = window.devicePixelRatio || 1;
    {{/display.high_dpi}}
        app_container.style.width = width + "px";
        app_container.style.height = height + buttonHeight + "px";
        game_canvas.width = Math.floor(width * dpi);
        game_canvas.height = Math.floor(height * dpi);
    }
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
    // do xhr request with retries
    request: function(url, method, responseType, currentAttempt) {
        if (typeof method === 'undefined') throw TypeError("No method specified");
        if (typeof method === 'responseType') throw TypeError("No responseType specified");
        if (typeof currentAttempt === 'undefined') currentAttempt = 0;
        var obj = {
            send: function() {
                var onprogress = this.onprogress;
                var onload = this.onload;
                var onerror = this.onerror;
                var onretry = this.onretry;

                var xhr = new XMLHttpRequest();
                xhr._loadedSize = 0;
                xhr.open(method, url, true);
                xhr.responseType = responseType;
                xhr.onprogress = function(event) {
                    if (onprogress) onprogress(xhr, event, xhr._loadedSize);
                    xhr._loadedSize = event.loaded;
                };
                xhr.onerror = function(event) {
                    if (currentAttempt == FileLoader.options.retryCount) {
                        if (onerror) onerror(xhr, event);
                        return;
                    }
                    if (onretry) onretry(xhr, event, xhr._loadedSize, currentAttempt);
                    xhr._loadedSize = 0;
                    currentAttempt += 1;
                    setTimeout(obj.send.bind(obj), FileLoader.options.retryInterval);
                };
                xhr.onload = function(event) {
                    if (onload) onload(xhr, event);
                };
                xhr.send(null);
            }
        };
        return obj;
    },
    // Do HTTP HEAD request to get size of resource
    // callback will receive size or undefined in case of an error
    size: function(url, callback) {
        var request = FileLoader.request(url, "HEAD", "text");
        request.onerror = function(xhr, e) {
            callback(undefined);
        };
        request.onload = function(xhr, e) {
            if (xhr.readyState === 4) {
                if (xhr.status === 200) {
                    var total = xhr.getResponseHeader('content-length');
                    callback(total);
                } else {
                    callback(undefined);
                }
            }
        };
        request.send();
    },
    // Do HTTP GET request
    // onprogress(loadedDelta)
    // onerror(error)
    // onload(response)
    // onretry(loadedSize, currentAttempt)
    load: function(url, responseType, onprogress, onerror, onload, onretry) {
        var request = FileLoader.request(url, "GET", responseType);
        request.onprogress = function(xhr, e, ls) {
            var delta = e.loaded - ls;
            onprogress(delta);
        };
        request.onerror = function(xhr, e) {
            onerror("Error loading '" + url + "' (" + e + ")");
        };
        request.onload = function(xhr, e) {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    var res = xhr.response;
                    if (responseType == "json" && typeof res === "string") {
                        onload(JSON.parse(res));
                    } else {
                        onload(res);
                    }
                } else {
                    onerror("Error loading '" + url + "' (" + e + ")");
                }
            }
        };
        request.onretry = function(xhr, event, loadedSize, currentAttempt) {
            onretry(loadedSize, currentAttempt);
        }
        request.send();
    }
};


var EngineLoader = {
    wasm_size: {{ DEFOLD_WASM_SIZE }},
    wasmjs_size: {{ DEFOLD_WASMJS_SIZE}},
    asmjs_size: {{ ASMJS_SIZE }},
    wasm_instantiate_progress: 0,

    stream_wasm: "{{html5.wasm_streaming}}" === "true",

    updateWasmInstantiateProgress: function(totalDownloadedSize) {
        EngineLoader.wasm_instantiate_progress = totalDownloadedSize * 0.1;
    },

    // load and instantiate .wasm file using XMLHttpRequest
    loadAndInstantiateWasmAsync: function(src, imports, successCallback) {
        FileLoader.load(src, "arraybuffer",
            function(delta) { 
                ProgressUpdater.updateCurrent(delta);
            },
            function(error) { throw error; },
            function(wasm) {
                if (wasm.byteLength != EngineLoader.wasm_size) {
                   console.warn("Unexpected wasm size:: " + wasm.byteLength + ", expected: " + EngineLoader.wasm_size);
                }
                var wasmInstantiate = WebAssembly.instantiate(new Uint8Array(wasm), imports).then(function(output) {
                    successCallback(output.instance);
                }).catch(function(e) {
                    console.log('wasm instantiation failed! ' + e);
                    throw e;
                });
            },
            function(loadedDelta, currentAttempt){
                ProgressUpdater.updateCurrent(-loadedDelta);
            });
    },

    // stream and instantiate .wasm file
    streamAndInstantiateWasmAsync: async function(src, imports, successCallback) {
        // https://stackoverflow.com/a/69179454
        var fetchFn = fetch;
        if (typeof TransformStream === "function" && ReadableStream.prototype.pipeThrough) {
            async function fetchWithProgress(path) {
                const response = await fetch(path);
                if (response.ok) {
                    const ts = new TransformStream({
                        transform (chunk, controller) {
                            ProgressUpdater.updateCurrent(chunk.byteLength);
                            controller.enqueue(chunk);
                        }
                    });

                    return new Response(response.body.pipeThrough(ts), response);
                } else {
                    return new Response(null, response);
                }
            }
            fetchFn = fetchWithProgress;
        }

        WebAssembly.instantiateStreaming(fetchFn(src), imports).then(function(output) {
            ProgressUpdater.updateCurrent(EngineLoader.wasm_instantiate_progress);
            successCallback(output.instance);
        }).catch(function(e) {
            console.log('wasm streaming instantiation failed! ' + e);
            console.log('Fallback to wasm loading');
            EngineLoader.loadAndInstantiateWasmAsync(src, imports, successCallback);
        });
    },

    // instantiate the .wasm file either by streaming it or first loading and then instantiate it
    // https://github.com/emscripten-core/emscripten/blob/main/test/manual_wasm_instantiate.html
    loadWasmAsync: function(exeName) {
        Module.instantiateWasm = function(imports, successCallback) {
            if (EngineLoader.stream_wasm && (typeof WebAssembly.instantiateStreaming === "function")) {
                EngineLoader.streamAndInstantiateWasmAsync(exeName + ".wasm", imports, successCallback);
            }
            else {
                EngineLoader.loadAndInstantiateWasmAsync(exeName + ".wasm", imports, successCallback);
            }
            return {}; // Compiling asynchronously, no exports.
        };
        EngineLoader.loadAndRunScriptAsync(exeName + '_wasm.js');
    },

    loadAsmJsAsync: function(exeName) {
        EngineLoader.loadAndRunScriptAsync(exeName + '_asmjs.js');
    },

    // load and start engine script (asm.js or wasm.js)
    loadAndRunScriptAsync: function(src) {
        FileLoader.load(src, "text",
            function(delta) {
                ProgressUpdater.updateCurrent(delta);
            },
            function(error) { throw error; },
            function(response) {
                var tag = document.createElement("script");
                tag.text = response;
                document.body.appendChild(tag);
            },
            function(loadedDelta, currentAttempt){
                ProgressUpdater.updateCurrent(-loadedDelta);
            });
    },

    // left as entrypoint for backward capability
    // start loading archive_files.json
    // after receiving it - start loading engine and data concurrently
    load: function(appCanvasId, exeName) {
        ProgressView.addProgress(Module.setupCanvas(appCanvasId));
        CUSTOM_PARAMETERS['exe_name'] = exeName;

        FileLoader.options.retryCount = CUSTOM_PARAMETERS["retry_count"];
        FileLoader.options.retryInterval = CUSTOM_PARAMETERS["retry_time"] * 1000;
        if (typeof CUSTOM_PARAMETERS["can_not_download_file_callback"] === "function") {
            GameArchiveLoader.addFileDownloadErrorListener(CUSTOM_PARAMETERS["can_not_download_file_callback"]);
        }
        // Load and assemble archive
        GameArchiveLoader.addFileLoadedListener(Module.onArchiveFileLoaded);
        GameArchiveLoader.addArchiveLoadedListener(Module.onArchiveLoaded);
        GameArchiveLoader.setFileLocationFilter(CUSTOM_PARAMETERS["archive_location_filter"]);
        GameArchiveLoader.loadArchiveDescription('/archive_files.json');

        // move resize callback setup here to make possible to override callback
        // from outside of dmloader.js
        if (typeof CUSTOM_PARAMETERS["resize_window_callback"] === "function") {
            var callback = CUSTOM_PARAMETERS["resize_window_callback"]
            callback();
            window.addEventListener('resize', callback, false);
            window.addEventListener('orientationchange', callback, false);
            window.addEventListener('focus', callback, false);
        }        
    }
}


/* ********************************************************************* */
/* Load and combine game archive data that is split into archives        */
/* ********************************************************************* */

var GameArchiveLoader = {
    // which files to load
    _files: [],
    _fileIndex: 0,
    // file
    //  name: intended filepath of built object
    //  size: expected size of built object.
    //  data: combined pieces
    //  downloaded: total bytes downloaded
    //  pieces: array of name, offset and data objects
    //  numExpectedFiles: total number of files expected in description
    //  lastRequestedPiece: index of last data file requested (strictly ascending)
    //  totalLoadedPieces: counts the number pieces received

    //MAX_CONCURRENT_XHR: 6,    // remove comment if throttling of XHR is desired.

    isCompleted: false,       // status of process

    _onFileLoadedListeners: [],          // signature: name, data.
    _onArchiveLoadedListeners:[],        // signature: void
    _onFileDownloadErrorListeners: [],   // signature: name

    _archiveLocationFilter: function(path) { return "split" + path; },

    cleanUp: function() {
        this._files =  [];
        this._fileIndex = 0;
        this.isCompleted = false;
        this._onGameArchiveLoaderCompletedListeners = [];
        this._onAllTargetsBuiltListeners = [];
        this._onFileDownloadErrorListeners = [];
    },

    addListener: function(list, callback) {
        if (typeof callback !== 'function') throw TypeError("Invalid callback registration");
        list.push(callback);
    },
    notifyListeners: function(list, data) {
        for (i=0; i<list.length; ++i) {
            list[i](data);
        }
    },

    addFileDownloadErrorListener: function(callback) {
        this.addListener(this._onFileDownloadErrorListeners, callback);
    },
    notifyFileDownloadError: function(url) {
        this.notifyListeners(this._onFileDownloadErrorListeners, url);
    },

    addFileLoadedListener: function(callback) {
        this.addListener(this._onFileLoadedListeners, callback);
    },
    notifyFileLoaded: function(file) {
        this.notifyListeners(this._onFileLoadedListeners, { name: file.name, data: file.data });
    },

    addArchiveLoadedListener: function(callback) {
        this.addListener(this._onArchiveLoadedListeners, callback);
    },
    notifyArchiveLoaded: function() {
        this.notifyListeners(this._onArchiveLoadedListeners);
    },

    setFileLocationFilter: function(filter) {
        if (typeof filter !== 'function') throw "Invalid filter";
        this._archiveLocationFilter = filter;
    },

    // load the archive_files.json with the list of files and their individual
    // pieces
    // descriptionUrl: location of text file describing files to be preloaded
    loadArchiveDescription: function(descriptionUrl) {
        FileLoader.load(
            this._archiveLocationFilter(descriptionUrl),
            "json",
            function (delta) { },
            function (error) { GameArchiveLoader.notifyFileDownloadError(descriptionUrl); },
            function (json) { GameArchiveLoader.onReceiveDescription(json); },
            function (loadedDelta, currentAttempt) { });
    },

    onReceiveDescription: function(json) {
        var totalSize = json.total_size;
        var exeName = CUSTOM_PARAMETERS['exe_name'];
        this._files = json.content;

        var isWASMSupported = Module['isWASMSupported'];
        if (isWASMSupported) {
            EngineLoader.loadWasmAsync(exeName);
            totalSize += EngineLoader.wasm_size + EngineLoader.wasmjs_size;
        } else {
            EngineLoader.loadAsmJsAsync(exeName);
            totalSize += EngineLoader.asmjs_size;
        }
        this.downloadContent();
        ProgressUpdater.resetCurrent();
        if (isWASMSupported) {
            EngineLoader.updateWasmInstantiateProgress(totalSize);
        }
        ProgressUpdater.setupTotal(totalSize + EngineLoader.wasm_instantiate_progress);
    },

    downloadContent: function() {
        var file = this._files[this._fileIndex];
        // if the file consists of more than one piece we prepare an array to store the pieces in
        if (file.pieces.length > 1) {
            file.data = new Uint8Array(file.size);
        }
        // how many pieces to download at a time
        var limit = file.pieces.length;
        if (typeof this.MAX_CONCURRENT_XHR !== 'undefined') {
            limit = Math.min(limit, this.MAX_CONCURRENT_XHR);
        }
        // download pieces
        for (var i=0; i<limit; ++i) {
            this.downloadPiece(file, i);
        }
    },

    notifyDownloadProgress: function(delta) {
        ProgressUpdater.updateCurrent(delta);
    },

    downloadPiece: function(file, index) {
        if (index < file.lastRequestedPiece) {
            throw RangeError("Request out of order: " + file.name + ", index: " + index + ", last requested piece: " + file.lastRequestedPiece);
        }

        var piece = file.pieces[index];
        file.lastRequestedPiece = index;
        file.totalLoadedPieces = 0;

        var url = this._archiveLocationFilter('/' + piece.name);

        FileLoader.load(
            url, "arraybuffer",
            function (delta) {
                GameArchiveLoader.notifyDownloadProgress(delta);
            },
            function (error) {
                GameArchiveLoader.notifyFileDownloadError(error);
            },
            function (response) {
                piece.data = new Uint8Array(response);
                piece.dataLength = piece.data.length;
                total = piece.dataLength;
                downloaded = piece.dataLength;
                GameArchiveLoader.onPieceLoaded(file, piece);
                piece.data = undefined;
            },
            function(loadedDelta, currentAttempt){
                ProgressUpdater.updateCurrent(-loadedDelta);
            });
    },

    addPieceToFile: function(file, piece) {
        if (1 == file.pieces.length) {
            file.data = piece.data;
        } else {
            var start = piece.offset;
            var end = start + piece.data.length;
            if (0 > start) {
                throw RangeError("Buffer underflow. Start: " + start);
            }
            if (end > file.data.length) {
                throw RangeError("Buffer overflow. End : " + end + ", data length: " + file.data.length);
            }
            file.data.set(piece.data, piece.offset);
        }
    },

    onPieceLoaded: function(file, piece) {
        this.addPieceToFile(file, piece);

        ++file.totalLoadedPieces;
        // is all pieces of the file loaded?
        if (file.totalLoadedPieces == file.pieces.length) {
            this.onFileLoaded(file);
        }
        // continue loading more pieces of the file
        // if not all pieces are already in progress
        else {
            var next = file.lastRequestedPiece + 1;
            if (next < file.pieces.length) {
                this.downloadPiece(file, next);
            }
        }
    },

    verifyFile: function(file) {
        // verify that we downloaded as much as we were supposed to
        var actualSize = 0;
        for (var i=0;i<file.pieces.length; ++i) {
            actualSize += file.pieces[i].dataLength;
        }
        if (actualSize != file.size) {
            throw "Unexpected data size: " + file.name + ", expected size: " + file.size + ", actual size: " + actualSize;
        }

        // verify the pieces
        if (file.pieces.length > 1) {
            var pieces = file.pieces;
            for (i=0; i<pieces.length; ++i) {
                var item = pieces[i];
                // Bounds check
                var start = item.offset;
                var end = start + item.dataLength;
                if (0 < i) {
                    var previous = pieces[i - 1];
                    if (previous.offset + previous.dataLength > start) {
                        throw RangeError("Segment underflow in file: " + file.name + ", offset: " + (previous.offset + previous.dataLength) + " , start: " + start);
                    }
                }
                if (pieces.length - 2 > i) {
                    var next = pieces[i + 1];
                    if (end > next.offset) {
                        throw RangeError("Segment overflow in file: " + file.name + ", offset: " + next.offset + ", end: " + end);
                    }
                }
            }
        }
    },

    onFileLoaded: function(file) {
        this.verifyFile(file);
        this.notifyFileLoaded(file);
        ++this._fileIndex;
        if (this._fileIndex == this._files.length) {
            this.onArchiveLoaded();
        } else {
            this.downloadContent();
        }
    },

    onArchiveLoaded: function() {
        this.isCompleted = true;
        this.notifyArchiveLoaded();
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
            Module.canvas.style.background = "";
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
        for (i=0; i<this.listeners.length; ++i) {
            this.listeners[i](percentage);
        }
    },

    setupTotal: function (total) {
        this.total = total;
    },

    setCurrent: function (current) {
        this.current = current;
        var percentage = this.calculateProgress();
        ProgressView.updateProgress(percentage);
        this.notifyListeners(percentage);
    },

    updateCurrent: function (diff) {
        this.current += diff;
        var percentage = this.calculateProgress();
        ProgressView.updateProgress(percentage);
        this.notifyListeners(percentage);
    },

    resetCurrent: function () {
        this.current = 0;
    },

    complete: function () {
        this.setCurrent(this.total);
    },

    calculateProgress: function () {
        return this.current / this.total * 100;
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
    noInitialRun: true,

    _filesToPreload: [],
    _archiveLoaded: false,
    _preLoadDone: false,
    _isEngineLoaded: false,

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

    hasWebGLSupport: function() {
        var webgl_support = false;
        try {
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
    runApp: function(appCanvasId, _) {
        Module._isEngineLoaded = true;
        Module.setupCanvas(appCanvasId);

        Module.arguments = CUSTOM_PARAMETERS["engine_arguments"];

        var fullScreenContainer = CUSTOM_PARAMETERS["full_screen_container"];
        if (typeof fullScreenContainer === "string") {
            fullScreenContainer = document.querySelector(fullScreenContainer);
        }
        Module.fullScreenContainer = fullScreenContainer || Module.canvas;

        if (Module.hasWebGLSupport()) {
            Module.canvas.focus();

            // Add context menu hide-handler if requested
            if (CUSTOM_PARAMETERS["disable_context_menu"])
            {
                Module.canvas.oncontextmenu = function(e) {
                    e.preventDefault();
                };
            }
            Module._preloadAndCallMain();
        } else {
            // "Unable to start game, WebGL not supported"
            ProgressUpdater.complete();
            Module.setStatus = function(text) {
                if (text) Module.printErr('[missing WebGL] ' + text);
            };

            if (typeof CUSTOM_PARAMETERS["unsupported_webgl_callback"] === "function") {
                CUSTOM_PARAMETERS["unsupported_webgl_callback"]();
            }
        }
    },

    onArchiveFileLoaded: function(file) {
        Module._filesToPreload.push({path: file.name, data: file.data});
    },

    onArchiveLoaded: function() {
        GameArchiveLoader.cleanUp();
        Module._archiveLoaded = true;
        Module._preloadAndCallMain();
    },

    toggleFullscreen: function(element) {
        if (GLFW.isFullscreen) {
            GLFW.cancelFullScreen();
        } else {
            GLFW.requestFullScreen(element);
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
                console.warn("Unable to synchronize mounted file systems: " + err);
                if (Module._syncMaxTries > Module._syncTries) {
                    Module.preSync(done);
                } else {
                    Module._syncInitial = true;
                    done();
                }
            } else {
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
            FS.createPreloadedFile("", item.path, item.data, true, true);
        }
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
        // Mount filesystem on preinit
        var dir = DMSYS.GetUserPersistentDataRoot();
        try {
            FS.mkdir(dir);
        }
        catch (error) {
            Module.persistentStorage = false;
            Module._preloadAndCallMain();
            return;
        }

        // If IndexedDB is supported we mount the persistent data root as IDBFS,
        // then try to do a IDB->MEM sync before we start the engine to get
        // previously saved data before boot.
        try {
            FS.mount(IDBFS, {}, dir);
            // Patch FS.close so it will try to sync MEM->IDB
            var _close = FS.close;
            FS.close = function(stream) {
                var r = _close(stream);
                Module.persistentSync();
                return r;
            }
        }
        catch (error) {
            Module.persistentStorage = false;
            Module._preloadAndCallMain();
            return;
        }

        // Sync IDB->MEM before calling main()
        Module.preSync(function() {
            Module._preloadAndCallMain();
        });
    }],

    preRun: [function() {
        /* If archive is loaded, preload all its files */
        if(Module._archiveLoaded) {
            Module.preloadAll();
        }
    }],

    postRun: [function() {
        if(Module._archiveLoaded) {
            ProgressView.removeProgress();
        }
    }],

    _preloadAndCallMain: function() {
        if (Module._syncInitial || Module.persistentStorage != true) {
            // If the archive isn't loaded,
            // we will have to wait with calling main.
            if (Module._archiveLoaded) {
                Module.preloadAll();
                if (Module._isEngineLoaded) {
                    // "Starting...."
                    ProgressUpdater.complete();
                    Module._callMain();
                }
            }
        }
    },

    _callMain: function() {
        ProgressView.removeProgress();
        if (Module.callMain === undefined) {
            Module.noInitialRun = false;
        } else {
            Module.callMain(Module.arguments);
        }
    },
    // Wrap IDBFS syncfs call with logic to avoid multiple syncs
    // running at the same time.
    _startSyncFS: function() {
        Module._syncInProgress = true;

        if (Module._syncMaxTries > Module._syncTries) {
            FS.syncfs(false, function(err) {
                Module._syncInProgress = false;

                if (err) {
                    console.warn("Unable to synchronize mounted file systems: " + err);
                    Module._syncTries += 1;
                }

                if (Module._syncNeeded) {
                    Module._syncNeeded = false;
                    Module._startSyncFS();
                }

            });
        }
    },
};

// common engine setup
Module['persistentStorage'] = (typeof window !== 'undefined') && !!(window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB);

Module['INITIAL_MEMORY'] = CUSTOM_PARAMETERS.custom_heap_size;

Module['onRuntimeInitialized'] = function() {
    Module.runApp("canvas");
};

Module["locateFile"] = function(path, scriptDirectory)
{
    // dmengine*.wasm is hardcoded in the built JS loader for WASM,
    // we need to replace it here with the correct project name.
    if (path == "dmengine.wasm" || path == "dmengine_release.wasm" || path == "dmengine_headless.wasm") {
        path = "{{exe-name}}.wasm";
    }
    return scriptDirectory + path;
};

{{^DEFOLD_HAS_WASM_ENGINE}}
Module["isWASMSupported"] = false; 
{{/DEFOLD_HAS_WASM_ENGINE}}

window.onerror = function(err, url, line, column, errObj) {
    if (typeof Module.ccall !== 'undefined') {
        var errorObject = Module.prepareErrorObject(err, url, line, column, errObj);
        Module.ccall('JSWriteDump', 'null', ['string'], [JSON.stringify(errorObject.stack)]);
    }
    Module.setStatus('Exception thrown, see JavaScript console');
    Module.setStatus = function(text) {
        if (text) Module.printErr('[post-exception status] ' + text);
    };
};
