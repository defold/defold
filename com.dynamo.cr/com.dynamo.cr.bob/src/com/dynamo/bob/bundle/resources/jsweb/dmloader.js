// file downloader
// wraps XMLHttpRequest and adds retry support and progress updates when the
// content is gzipped (gzipped content doesn't report a computable content length
// on Google Chrome)
var FileLoader = {
    progressHandler: function(fromProgress, toProgress) {
        return function(loaded, total) {
            Progress.calculateProgress(fromProgress, toProgress, loaded, total);
        };
    },
    errorHandler: function(context) {
        return function(error) {
            throw context + ": " + error;
        };
    },
    load: function(url, onprogress, onerror, onload, options, currentAttempt) {
        if (typeof options === 'undefined') options = {};
        if (typeof options.responseType === 'undefined') options.responseType = "";
        if (typeof options.retryCount === 'undefined') options.retryCount = 1;
        if (typeof options.retryInterval === 'undefined') options.retryInterval = 1000;
        if (typeof currentAttempt === 'undefined') currentAttempt = 0;

        var unknown_total = 0;
        var unknown_multiplier = 10;
        var xhr = new XMLHttpRequest();
        xhr.open('GET', url, true);
        xhr.responseType = options.responseType;
        xhr.onprogress = function(e) {
            if (e.lengthComputable) {
                onprogress(e.loaded, e.total);
                return;
            }
            var total = xhr.getResponseHeader('content-length')
            var encoding = xhr.getResponseHeader('content-encoding')
            if (total && encoding && (encoding.indexOf('gzip') > -1)) {
                total /= 0.4; // assuming 40% compression rate
                onprogress(e.loaded, total);
            } else {
                if (e.loaded > unknown_total) {
                    unknown_total = e.loaded * unknown_multiplier;
                    unknown_multiplier = 1 + unknown_multiplier * 0.2;
                }
                onprogress(e.loaded, unknown_total);
            }
        };
        xhr.onload = function(e) {
            if (xhr.readyState === 4) {
                if (xhr.status === 200) {
                    onload(xhr.response);
                } else {
                    onerror("Error loading '" + url + "' (" + e + ")");
                }
            }
        };
        xhr.onerror = function(e) {
            if (currentAttempt < options.retryCount) {
                setTimeout(function() {
                    FileLoader.load(url, onprogress, onloaded, onerror, options, currentAttempt + 1);
                }, options.retryInterval);
            } else {
                onerror("Error loading '" + url + "' (" + e + ")");
            }
        };
        xhr.send(null);
    }
};


var EngineLoader = {
    // https://github.com/emscripten-core/emscripten/blob/master/tests/manual_wasm_instantiate.html#L170
    loadWasmAsync: function(src, fromProgress, toProgress, callback) {
        FileLoader.load(
            src,
            FileLoader.progressHandler(fromProgress, toProgress),
            FileLoader.errorHandler("WASM"),
            function (wasm) {
                console.log("wasm " + wasm);
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
            },
            {
                retryCount: 4,
                retryInterval: 1000,
                responseType: 'arraybuffer'
            });
    },
    loadScriptAsync: function(src, fromProgress, toProgress) {
        FileLoader.load(
            src,
            FileLoader.progressHandler(fromProgress, toProgress),
            FileLoader.errorHandler("ENGINE.JS"),
            function (response) {
                var tag = document.createElement("script");
                tag.text = response;
                document.head.appendChild(tag);
            },
            {
                retryCount: 4,
                retryInterval: 1000,
            });
    },

    // load engine (asm.js or wasm.js + wasm)
    // engine load progress goes from 1-50% for ams.js
    // engine load progress goes from 0-40% for .wasm and 40-50% for wasm.js
    load: function(appCanvasId, exeName) {
        Progress.addProgress(Module.setupCanvas(appCanvasId));
        if (Module['isWASMSupported']) {
            EngineLoader.loadWasmAsync(exeName + ".wasm", 0, 40, function(wasm) {
                EngineLoader.loadScriptAsync(exeName + '_wasm.js', 40, 50);
            });
        } else {
            EngineLoader.loadScriptAsync(exeName + '_asmjs.js', 0, 50);
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

    _retryTime: 0,             // pause before retry file loading after error
    _maxRetryCount: 0,        // how many attempts we do when trying to download a file.

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
    notifyListeners: function(list, ...args) {
        for (i=0; i<list.length; ++i) {
            list[i](...args);
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
        this.notifyListeners(this._onFileLoadedListeners, file.name, file.data);
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

    configureRetries: function(count, time) {
        this._retryTime = time;
        this._maxRetryCount = count;
    },

    // load the archive_files.json with the list of files and their individual
    // pieces
    // descriptionUrl: location of text file describing files to be preloaded
    loadArchiveDescription: function(descriptionUrl) {
        FileLoader.load(
            this._archiveLocationFilter(descriptionUrl),
            function (loaded, total) { },
            function (error) { GameArchiveLoader.notifyFileDownloadError(descriptionUrl); },
            function (response) { GameArchiveLoader.onReceiveDescription(response); },
            {
                responseType: 'text',
                retryCount: GameArchiveLoader._maxRetryCount,
                retryInterval: GameArchiveLoader._retryTime * 1000
            }
        );
    },

    onReceiveDescription: function(description) {
        var json = JSON.parse(description);
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
            url,
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
            },
            {
                responseType: 'arraybuffer',
                retryCount: GameArchiveLoader._maxRetryCount,
                retryInterval: GameArchiveLoader._retryTime * 1000
            }
        );
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
            Progress.bar.style.width = percentage + "%";
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
/* Default input override                                                */
/* ********************************************************************* */

var CanvasInput = {
    arrowKeysHandler : function(e) {
        switch(e.keyCode) {
            case 37: case 38: case 39:  case 40: // Arrow keys
            case 32: e.preventDefault(); e.stopPropagation(); // Space
            default: break; // do not block other keys
        }
    },

    onFocusIn : function(e) {
        window.addEventListener("keydown", CanvasInput.arrowKeysHandler, false);
    },

    onFocusOut: function(e) {
        window.removeEventListener("keydown", CanvasInput.arrowKeysHandler, false);
    },

    addToCanvas : function(canvas) {
        canvas.addEventListener("focus", CanvasInput.onFocusIn, false);
        canvas.addEventListener("blur", CanvasInput.onFocusOut, false);
        canvas.focus();
        CanvasInput.onFocusIn();
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

    handleVisibilityChange: function () {
        GLFW.onFocusChanged(document[Module.hiddenProperty] ? 0 : 1);
    },

    getHiddenProperty: function () {
        if ('hidden' in document) return 'hidden';
        var prefixes = ['webkit','moz','ms','o'];
        for (var i = 0; i < prefixes.length; i++) {
            if ((prefixes[i] + 'Hidden') in document)
                return prefixes[i] + 'Hidden';
        }
        return null;
    },

    setupVisibilityChangeListener: function() {
        Module.hiddenProperty = Module.getHiddenProperty();
        if( Module.hiddenProperty ) {
            var eventName = Module.hiddenProperty.replace(/[H|h]idden/,'') + 'visibilitychange';
            document.addEventListener(eventName, Module.handleVisibilityChange, false);
        } else {
            console.log("No document.hidden property found. The focus events won't be enabled.")
        }
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
        Module["TOTAL_MEMORY"] = params["custom_heap_size"];

        if (Module.hasWebGLSupport()) {
            // Override game keys
            CanvasInput.addToCanvas(Module.canvas);

            Module.setupVisibilityChangeListener();

            // Add context menu hide-handler if requested
            if (params["disable_context_menu"])
            {
                Module.canvas.oncontextmenu = function(e) {
                    e.preventDefault();
                };
            }

            GameArchiveLoader.configureRetries(params["retry_count"], params["retry_time"]);
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

    onArchiveFileLoaded: function(name, data) {
        Module._filesToPreload.push({path: name, data: data});
    },

    onArchiveLoaded: function() {
        GameArchiveLoader.cleanUp();
        Module._archiveLoaded = true;
        Progress.updateProgress(100, "Starting...");

        if (Module._waitingForArchive) {
            Module._preloadAndCallMain();
        }
    },

    toggleFullscreen: function() {
        if (GLFW.isFullscreen) {
            GLFW.cancelFullScreen();
        } else {
            GLFW.requestFullScreen();
        }
    },

    preSync: function(done) {
        // Initial persistent sync before main is called
        FS.syncfs(true, function(err) {
            if(err) {
                Module._syncTries += 1;
                console.error("FS syncfs error: " + err);
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
        /* Mount filesystem on preinit */
        var dir = DMSYS.GetUserPersistentDataRoot();
        FS.mkdir(dir);

        // If IndexedDB is supported we mount the persistent data root as IDBFS,
        // then try to do a IDB->MEM sync before we start the engine to get
        // previously saved data before boot.
        window.indexedDB = window.indexedDB || window.mozIndexedDB || window.webkitIndexedDB || window.msIndexedDB;
        if (Module.persistentStorage && window.indexedDB) {
            FS.mount(IDBFS, {}, dir);

            // Patch FS.close so it will try to sync MEM->IDB
            var _close = FS.close; FS.close = function(stream) { var r = _close(stream); Module.persistentSync(); return r; }

            // Sync IDB->MEM before calling main()
            Module.preSync(function() {
                Module._preloadAndCallMain();
            });
        } else {
            Module._preloadAndCallMain();
        }
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

            // Need to set heap size before calling main
            TOTAL_MEMORY = Module["TOTAL_MEMORY"] || TOTAL_MEMORY;

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
                    console.error("Module._startSyncFS error: " + err);
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
