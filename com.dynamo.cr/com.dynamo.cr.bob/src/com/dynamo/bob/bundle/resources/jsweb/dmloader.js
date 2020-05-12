/* ********************************************************************* */
/* Load and combine data that is split into archives                     */
/* ********************************************************************* */

var Combine = {
    _targets: [],
    _targetIndex: 0,
    // target: build target
    //  name: intended filepath of built object
    //  size: expected size of built object.
    //  data: combined data
    //  downloaded: total amount of data downloaded
    //  pieces: array of name, offset and data objects
    //  numExpectedFiles: total number of files expected in description
    //  lastRequestedPiece: index of last data file requested (strictly ascending)
    //  totalLoadedPieces: counts the number of data files received

    //MAX_CONCURRENT_XHR: 6,    // remove comment if throttling of XHR is desired.

    isCompleted: false,       // status of process

    _onCombineCompleted: [],    // signature: name, data.
    _onAllTargetsBuilt:[],      // signature: void
    _onDownloadProgress: [],    // signature: downloaded, total

    _currentDownloadBytes: 0,
    _totalDownloadBytes: 0,

    _retry_time: 0,             // pause before retry file loading after error
    _max_retry_count: 0,        // how many attempts we do when trying to download a file.
    _can_not_download_file_callback: undefined, //Function that is called if you can't download file after 'retry_count' attempts.

    _archiveLocationFilter: function(path) { return "split" + path; },

    can_not_download_file: function(file) {
        if (typeof Combine._can_not_download_file_callback === 'function') {
            Combine._can_not_download_file_callback(file);
        }
    },

    addProgressListener: function(callback) {
        if (typeof callback !== 'function') {
            throw "Invalid callback registration";
        }
        this._onDownloadProgress.push(callback);
    },

    addCombineCompletedListener: function(callback) {
        if (typeof callback !== 'function') {
            throw "Invalid callback registration";
        }
        this._onCombineCompleted.push(callback);
    },

    addAllTargetsBuiltListener: function(callback) {
        if (typeof callback !== 'function') {
            throw "Invalid callback registration";
        }
        this._onAllTargetsBuilt.push(callback);
    },

    // descriptUrl: location of text file describing files to be preloaded
    process: function(descriptUrl, attempt_count) {
        if (!attempt_count) {
            attempt_count = 0;
        }
        var xhr = new XMLHttpRequest();
        xhr.open('GET', descriptUrl);
        xhr.responseType = 'text';
        xhr.onload = function(evt) {
            Combine.onReceiveDescription(xhr);
        };
        xhr.onerror = function(evt) {
            attempt_count += 1;
            if (attempt_count < Combine._max_retry_count) {
                console.warn("Can't download file '" + descriptUrl + "' . Next try in " + Combine._retry_time + " sec.");
                setTimeout(function() {
                    Combine.process(descriptUrl, attempt_count);
                }, Combine._retry_time * 1000);
             } else {
                    Combine.can_not_download_file(descriptUrl);
            }
        };
        xhr.send(null);
    },

    cleanUp: function() {
        this._targets =  [];
        this._targetIndex = 0;
        this.isCompleted = false;
        this._onCombineCompleted = [];
        this._onAllTargetsBuilt = [];
        this._onDownloadProgress = [];

        this._currentDownloadBytes = 0;
        this._totalDownloadBytes = 0;
    },

    onReceiveDescription: function(xhr) {
        var json = JSON.parse(xhr.responseText);
        this._targets = json.content;
        this._totalDownloadBytes = 0;
        this._currentDownloadBytes = 0;

        var targets = this._targets;
        for(var i=0; i<targets.length; ++i) {
            this._totalDownloadBytes += targets[i].size;
        }
        this.requestContent();
    },

    requestContent: function() {
        var target = this._targets[this._targetIndex];
        if (1 < target.pieces.length) {
            target.data = new Uint8Array(target.size);
        }
        var limit = target.pieces.length;
        if (typeof this.MAX_CONCURRENT_XHR !== 'undefined') {
            limit = Math.min(limit, this.MAX_CONCURRENT_XHR);
        }
        for (var i=0; i<limit; ++i) {
            this.requestPiece(target, i);
        }
    },

    requestPiece: function(target, index, attempt_count) {
        if (!attempt_count) {
            attempt_count = 0;
        }

        if (index <  target.lastRequestedPiece) {
            throw "Request out of order";
        }

        var item = target.pieces[index];
        target.lastRequestedPiece = index;

        var total = 0;
        var downloaded = 0;
        var xhr = new XMLHttpRequest();
        var url = this._archiveLocationFilter('/' + item.name);
        xhr.open('GET', url, true);
        xhr.responseType = 'arraybuffer';
        // called periodically with information about the transaction
        xhr.onprogress = function(evt) {
            if (evt.total && evt.lengthComputable) {
                total = evt.total;
            }
            if (evt.loaded && evt.lengthComputable) {
                var delta = evt.loaded - downloaded;
                downloaded = evt.loaded;
                Combine._currentDownloadBytes += delta;
                Combine.updateProgress(target);
            }
        };
        // called when the transaction completes successfully
        xhr.onload = function(evt) {
            item.data = new Uint8Array(xhr.response);
            item.dataLength = item.data.length;
            total = item.dataLength;
            downloaded = item.dataLength;
            Combine.copyData(target, item);
            Combine.onPieceLoaded(target, item);
            Combine.updateProgress(target);
            item.data = undefined;
        };
        // called when the transaction fails
        xhr.onerror = function(evt) {
            downloaded = 0;
            Combine.updateProgress(target);
            attempt_count += 1;
            if (attempt_count < Combine._max_retry_count) {
                console.warn("Can't download file '" + item.name + "' . Next try in " + Combine._retry_time + " sec.");
                setTimeout(function() {
                    Combine.requestPiece(target, index, attempt_count);
                }, Combine._retry_time * 1000);
            } else {
                Combine.can_not_download_file(item.name);
            }
        };
        xhr.send(null);
    },

    updateProgress: function(target) {
        for(i = 0; i<this._onDownloadProgress.length; ++i) {
            this._onDownloadProgress[i](this._currentDownloadBytes, this._totalDownloadBytes);
        }
    },

    copyData: function(target, item) {
        if (1 == target.pieces.length) {
            target.data = item.data;
        } else {
            var start = item.offset;
            var end = start + item.data.length;
            if (0 > start) {
                throw "Buffer underflow";
            }
            if (end > target.data.length) {
                throw "Buffer overflow";
            }
            target.data.set(item.data, item.offset);
        }
    },

    onPieceLoaded: function(target, item) {
        if (typeof target.totalLoadedPieces === 'undefined') {
            target.totalLoadedPieces = 0;
        }
        ++target.totalLoadedPieces;
        if (target.totalLoadedPieces == target.pieces.length) {
            this.finalizeTarget(target);
            ++this._targetIndex;
            for (var i=0; i<this._onCombineCompleted.length; ++i) {
                this._onCombineCompleted[i](target.name, target.data);
            }
            if (this._targetIndex < this._targets.length) {
                this.requestContent();
            } else {
                this.isCompleted = true;
                for (i=0; i<this._onAllTargetsBuilt.length; ++i) {
                    this._onAllTargetsBuilt[i]();
                }
            }
        } else {
            var next = target.lastRequestedPiece + 1;
            if (next < target.pieces.length) {
                this.requestPiece(target, next);
            }
        }
    },

    finalizeTarget: function(target) {
        var actualSize = 0;
        for (var i=0;i<target.pieces.length; ++i) {
            actualSize += target.pieces[i].dataLength;
        }
        if (actualSize != target.size) {
            throw "Unexpected data size";
        }

        if (1 < target.pieces.length) {
            var output = target.data;
            var pieces = target.pieces;
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
    }
};

/* ********************************************************************* */
/* Default splash and progress visualisation                             */
/* ********************************************************************* */

var Progress = {
    progress_id: "defold-progress",
    bar_id: "defold-progress-bar",

    addProgress : function (canvas) {
        /* Insert default progress bar below canvas */
        canvas.insertAdjacentHTML('afterend', '<div id="' + Progress.progress_id + '" class="canvas-app-progress"><div id="' + Progress.bar_id + '" class="canvas-app-progress-bar" style="width: 0%;"></div></div>');
        Progress.bar = document.getElementById(Progress.bar_id);
        Progress.progress = document.getElementById(Progress.progress_id);
    },

    updateProgress: function (percentage, text) {
        Progress.bar.style.width = percentage + "%";
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
            if (typeof WebAssembly === "object"
                && typeof WebAssembly.instantiate === "function") {
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
    runApp: function(app_canvas_id, extra_params) {
        app_canvas_id = (typeof app_canvas_id === 'undefined') ?  'canvas' : app_canvas_id;

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

        Module.canvas = document.getElementById(app_canvas_id);
        Module.arguments = params["engine_arguments"];
        Module.persistentStorage = params["persistent_storage"];
        Module["TOTAL_MEMORY"] = params["custom_heap_size"];

        if (Module.hasWebGLSupport()) {
            // Override game keys
            CanvasInput.addToCanvas(Module.canvas);

            Module.setupVisibilityChangeListener();

            // Add progress visuals
            Progress.addProgress(Module.canvas);

            // Add context menu hide-handler if requested
            if (params["disable_context_menu"])
            {
                Module.canvas.oncontextmenu = function(e) {
                    e.preventDefault();
                };
            }

            Combine._retry_time = params["retry_time"];
            Combine._max_retry_count = params["retry_count"];
            if (typeof params["can_not_download_file_callback"] === "function") {
                Combine._can_not_download_file_callback = params["can_not_download_file_callback"];
            }
            // Load and assemble archive
            Combine.addCombineCompletedListener(Module.onArchiveFileLoaded);
            Combine.addAllTargetsBuiltListener(Module.onArchiveLoaded);
            Combine.addProgressListener(Module.onArchiveLoadProgress);
            Combine._archiveLocationFilter = params["archive_location_filter"];
            Combine.process(Combine._archiveLocationFilter('/archive_files.json'));
        } else {
            Progress.addProgress(Module.canvas);
            Progress.updateProgress(100, "Unable to start game, WebGL not supported");
            Module.setStatus = function(text) {
                if (text) Module.printErr('[missing WebGL] ' + text);
            };

            if (typeof params["unsupported_webgl_callback"] === "function") {
                params["unsupported_webgl_callback"]();
            }
        }
    },

    onArchiveLoadProgress: function(downloaded, total) {
        Progress.updateProgress(downloaded / total * 100);
    },

    onArchiveFileLoaded: function(name, data) {
        Module._filesToPreload.push({path: name, data: data});
    },

    onArchiveLoaded: function() {
        Combine.cleanUp();
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
    var errorObject = Module.prepareErrorObject(err, url, line, column, errObj);
    Module.ccall('JSWriteDump', 'null', ['string'], [JSON.stringify(errorObject.stack)]);
    Module.setStatus('Exception thrown, see JavaScript console');
    Module.setStatus = function(text) {
        if (text) Module.printErr('[post-exception status] ' + text);
    };
};
