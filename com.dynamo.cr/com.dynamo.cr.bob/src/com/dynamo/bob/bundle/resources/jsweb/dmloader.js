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
        if (typeof method === 'undefined') throw "No method specified";
        if (typeof method === 'responseType') throw "No responseType specified";
        if (typeof currentAttempt === 'undefined') currentAttempt = 0;
        var obj = {
            send: function() {
                var onprogress = this.onprogress;
                var onload = this.onload;
                var onerror = this.onerror;

                var xhr = new XMLHttpRequest();
                xhr.open(method, url, true);
                xhr.responseType = responseType;
                xhr.onprogress = function(e) {
                    if (onprogress) onprogress(xhr, e);
                };
                xhr.onerror = function(e) {
                    if (currentAttempt == FileLoader.options.retryCount) {
                        if (onerror) onerror(xhr, e);
                        return;
                    }
                    currentAttempt = currentAttempt + 1;
                    setTimeout(obj.send.bind(obj), FileLoader.options.retryInterval);
                };
                xhr.onload = function(e) {
                    if (onload) onload(xhr, e);
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
    // onprogress(loaded, total)
    // onerror(error)
    // onload(response)
    load: function(url, responseType, estimatedSize, onprogress, onerror, onload) {
        var request = FileLoader.request(url, "GET", responseType);
        request.onprogress = function(xhr, e) {
            if (e.lengthComputable) {
                onprogress(e.loaded, e.total);
                return;
            }
            var contentLength = xhr.getResponseHeader('content-length');
            var size = contentLength != undefined ? contentLength : estimatedSize;
            if (size) {
                onprogress(e.loaded, size);
            } else {
                onprogress(e.loaded, e.loaded);
            }
        };
        request.onerror = function(xhr, e) {
            onerror("Error loading '" + url + "' (" + e + ")");
        };
        request.onload = function(xhr, e) {
            if (xhr.readyState === 4) {
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
        request.send();
    }
};


var EngineLoader = {
    wasm_size: 2000000,
    wasmjs_size: 250000,
    asmjs_size: 4000000,

    stream_wasm: false,

    loadAndInstantiateWasmAsync: function(src, fromProgress, toProgress, callback) {
        FileLoader.load(src, "arraybuffer", EngineLoader.wasm_size,
            function(loaded, total) { Progress.calculateProgress(fromProgress, toProgress, loaded, total); },
            function(error) { throw error; },
            function(wasm) {
                Module.instantiateWasm = function(imports, successCallback) {
                    var wasmInstantiate = WebAssembly.instantiate(new Uint8Array(wasm), imports).then(function(output) {
                        successCallback(output.instance);
                    }).catch(function(e) {
                        console.log('wasm instantiation failed! ' + e);
                        throw e;
                    });
                    return {}; // Compiling asynchronously, no exports.
                }
                callback();
            });
    },

    setupWasmStreamAsync: async function(src, fromProgress, toProgress) {
        // https://stackoverflow.com/a/69179454
        var fetchFn = fetch;
        if (typeof TransformStream === "function" && ReadableStream.prototype.pipeThrough) {
            async function fetchWithProgress(path) {
                const response = await fetch(path);
                // May be incorrect if compressed
                var contentLength = response.headers.get("Content-Length");
                if (!contentLength){
                    contentLength = EngineLoader.wasm_size;
                }
                const total = parseInt(contentLength, 10);

                let bytesLoaded = 0;
                const ts = new TransformStream({
                    transform (chunk, controller) {
                        bytesLoaded += chunk.byteLength;
                        Progress.calculateProgress(fromProgress, toProgress, bytesLoaded, total);
                        controller.enqueue(chunk)
                    }
                });

                return new Response(response.body.pipeThrough(ts), response);
            }
            fetchFn = fetchWithProgress;
        }

        Module.instantiateWasm = function(imports, successCallback) {
            WebAssembly.instantiateStreaming(fetchFn(src), imports).then(function(output) {
                Progress.calculateProgress(fromProgress, toProgress, 1, 1);
                successCallback(output.instance);
            }).catch(function(e) {
                console.log('wasm streaming instantiation failed! ' + e);
                throw e;
            });
            return {}; // Compiling asynchronously, no exports.
        }
    },

    // instantiate the .wasm file either by streaming it or first loading and then instantiate it
    // https://github.com/emscripten-core/emscripten/blob/master/tests/manual_wasm_instantiate.html#L170
    loadWasmAsync: function(exeName) {
        if (EngineLoader.stream_wasm && (typeof WebAssembly.instantiateStreaming === "function")) {
            EngineLoader.setupWasmStreamAsync(exeName + ".wasm", 10, 50);
            EngineLoader.loadAndRunScriptAsync(exeName + '_wasm.js', EngineLoader.wasmjs_size, 0, 10);
        }
        else {
            EngineLoader.loadAndInstantiateWasmAsync(exeName + ".wasm", 0, 40, function() {
                EngineLoader.loadAndRunScriptAsync(exeName + '_wasm.js', EngineLoader.wasmjs_size, 40, 50);
            });
        }
    },

    loadAsmJsAsync: function(exeName) {
        EngineLoader.loadAndRunScriptAsync(exeName + '_asmjs.js', EngineLoader.asmjs_size, 0, 50);
    },

    // load and start engine script (asm.js or wasm.js)
    loadAndRunScriptAsync: function(src, estimatedSize, fromProgress, toProgress) {
        FileLoader.load(src, "text", estimatedSize,
            function(loaded, total) { Progress.calculateProgress(fromProgress, toProgress, loaded, total); },
            function(error) { throw error; },
            function(response) {
                var tag = document.createElement("script");
                tag.text = response;
                document.body.appendChild(tag);
            });
    },

    // load engine (asm.js or wasm.js + wasm)
    load: function(appCanvasId, exeName) {
        Progress.addProgress(Module.setupCanvas(appCanvasId));
        if (Module['isWASMSupported']) {
            EngineLoader.loadWasmAsync(exeName);
        } else {
            EngineLoader.loadAsmJsAsync(exeName);
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

    _currentDownloadBytes: 0,
    _totalDownloadBytes: 0,

    _archiveLocationFilter: function(path) { return "split" + path; },

    cleanUp: function() {
        this._files =  [];
        this._fileIndex = 0;
        this.isCompleted = false;
        this._onGameArchiveLoaderCompletedListeners = [];
        this._onAllTargetsBuiltListeners = [];
        this._onFileDownloadErrorListeners = [];

        this._currentDownloadBytes = 0;
        this._totalDownloadBytes = 0;
    },

    addListener: function(list, callback) {
        if (typeof callback !== 'function') throw "Invalid callback registration";
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
            undefined,
            function (loaded, total) { },
            function (error) { GameArchiveLoader.notifyFileDownloadError(descriptionUrl); },
            function (json) { GameArchiveLoader.onReceiveDescription(json); });
    },

    onReceiveDescription: function(json) {
        this._files = json.content;
        this._totalDownloadBytes = 0;
        this._currentDownloadBytes = 0;

        // calculate total download size of all files
        for(var i=0; i<this._files.length; ++i) {
            this._totalDownloadBytes += this._files[i].size;
        }
        this.downloadContent();
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

    notifyDownloadProgress: function() {
        Progress.calculateProgress(50, 100, this._currentDownloadBytes, this._totalDownloadBytes);
    },

    downloadPiece: function(file, index) {
        if (index < file.lastRequestedPiece) {
            throw "Request out of order";
        }

        var piece = file.pieces[index];
        file.lastRequestedPiece = index;
        file.totalLoadedPieces = 0;

        var total = 0;
        var downloaded = 0;
        var url = this._archiveLocationFilter('/' + piece.name);

        FileLoader.load(
            url, "arraybuffer", undefined,
            function (loaded, total) {
                var delta = loaded - downloaded;
                downloaded = loaded;
                GameArchiveLoader._currentDownloadBytes += delta;
                GameArchiveLoader.notifyDownloadProgress();
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
                GameArchiveLoader.notifyDownloadProgress();
                piece.data = undefined;
            });
    },

    addPieceToFile: function(file, piece) {
        if (1 == file.pieces.length) {
            file.data = piece.data;
        } else {
            var start = piece.offset;
            var end = start + piece.data.length;
            if (0 > start) {
                throw "Buffer underflow";
            }
            if (end > file.data.length) {
                throw "Buffer overflow";
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
            throw "Unexpected data size";
        }

        // verify the pieces
        if (file.pieces.length > 1) {
            var output = file.data;
            var pieces = file.pieces;
            for (i=0; i<pieces.length; ++i) {
                var item = pieces[i];
                // Bounds check
                var start = item.offset;
                var end = start + item.dataLength;
                if (0 < i) {
                    var previous = pieces[i - 1];
                    if (previous.offset + previous.dataLength > start) {
                        throw "Segment underflow";
                    }
                }
                if (pieces.length - 2 > i) {
                    var next = pieces[i + 1];
                    if (end > next.offset) {
                        throw "Segment overflow";
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

var Progress = {
    progress_id: "defold-progress",
    bar_id: "defold-progress-bar",

    listeners: [],

    addListener: function(callback) {
        if (typeof callback !== 'function') throw "Invalid callback registration";
        this.listeners.push(callback);
    },

    notifyListeners: function(percentage) {
        for (i=0; i<this.listeners.length; ++i) {
            this.listeners[i](percentage);
        }
    },

    addProgress : function (canvas) {
        /* Insert default progress bar below canvas */
        canvas.insertAdjacentHTML('afterend', '<div id="' + Progress.progress_id + '" class="canvas-app-progress"><div id="' + Progress.bar_id + '" class="canvas-app-progress-bar" style="width: 0%;"></div></div>');
        Progress.bar = document.getElementById(Progress.bar_id);
        Progress.progress = document.getElementById(Progress.progress_id);
    },

    updateProgress: function(percentage) {
        if (Progress.bar) {
            Progress.bar.style.width = Math.min(percentage, 100) + "%";
        }
        Progress.notifyListeners(percentage);
    },

    calculateProgress: function (from, to, current, total) {
        this.updateProgress(from + (current / total) * (to - from));
    },

    removeProgress: function () {
        if (Progress.progress.parentElement !== null) {
            Progress.progress.parentElement.removeChild(Progress.progress);

            // Remove any background/splash image that was set in runApp().
            // Workaround for Safari bug DEF-3061.
            Module.canvas.style.background = "";
        }
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
    _waitingForArchive: false,

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
        var backtrace = "";

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
    *
    * 'extra_params' is an optional object that can have the following fields:
    *
    *     'archive_location_filter':
    *         Filter function that will run for each archive path.
    *
    *     'unsupported_webgl_callback':
    *         Function that is called if WebGL is not supported.
    *
    *     'engine_arguments':
    *         List of arguments (strings) that will be passed to the engine.
    *
    *     'persistent_storage':
    *         Boolean toggling the usage of persistent storage.
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
    **/
    runApp: function(appCanvasId, extra_params) {
        Module.setupCanvas(appCanvasId);

        var params = {
            archive_location_filter: function(path) { return 'split' + path; },
            unsupported_webgl_callback: undefined,
            engine_arguments: [],
            persistent_storage: true,
            custom_heap_size: undefined,
            disable_context_menu: true,
            retry_time: 1,
            retry_count: 10,
            can_not_download_file_callback: undefined,
        };

        for (var k in extra_params) {
            if (extra_params.hasOwnProperty(k)) {
                params[k] = extra_params[k];
            }
        }

        Module.arguments = params["engine_arguments"];
        Module.persistentStorage = params["persistent_storage"];

        var fullScreenContainer = params["full_screen_container"];
        if (typeof fullScreenContainer === "string") {
            fullScreenContainer = document.querySelector(fullScreenContainer);
        }
        Module.fullScreenContainer = fullScreenContainer || Module.canvas;

        if (Module.hasWebGLSupport()) {
            Module.canvas.focus();

            // Add context menu hide-handler if requested
            if (params["disable_context_menu"])
            {
                Module.canvas.oncontextmenu = function(e) {
                    e.preventDefault();
                };
            }

            FileLoader.options.retryCount = params["retry_count"];
            FileLoader.options.retryInterval = params["retry_time"] * 1000;
            if (typeof params["can_not_download_file_callback"] === "function") {
                GameArchiveLoader.addFileDownloadErrorListener(params["can_not_download_file_callback"]);
            }
            // Load and assemble archive
            GameArchiveLoader.addFileLoadedListener(Module.onArchiveFileLoaded);
            GameArchiveLoader.addArchiveLoadedListener(Module.onArchiveLoaded);
            GameArchiveLoader.setFileLocationFilter(params["archive_location_filter"]);
            GameArchiveLoader.loadArchiveDescription('/archive_files.json');
        } else {
            Progress.updateProgress(100, "Unable to start game, WebGL not supported");
            Module.setStatus = function(text) {
                if (text) Module.printErr('[missing WebGL] ' + text);
            };

            if (typeof params["unsupported_webgl_callback"] === "function") {
                params["unsupported_webgl_callback"]();
            }
        }
    },

    onArchiveFileLoaded: function(file) {
        Module._filesToPreload.push({path: file.name, data: file.data});
    },

    onArchiveLoaded: function() {
        GameArchiveLoader.cleanUp();
        Module._archiveLoaded = true;
        Progress.updateProgress(100, "Starting...");

        if (Module._waitingForArchive) {
            Module._preloadAndCallMain();
        }
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
            Module._syncInitial = true;
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
            Progress.removeProgress();
        }
    }],

    _preloadAndCallMain: function() {
        // If the archive isn't loaded,
        // we will have to wait with calling main.
        if (!Module._archiveLoaded) {
            Module._waitingForArchive = true;
        } else {
            Module.preloadAll();
            Progress.removeProgress();
            if (Module.callMain === undefined) {
                Module.noInitialRun = false;
            } else {
                Module.callMain(Module.arguments);
            }
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
