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

    //MAX_CONCURRENT_XHR: 6,	// remove comment if throttling of XHR is desired.

    isCompleted: false,       // status of process

    _onCombineCompleted: [],    // signature: name, data.
    _onAllTargetsBuilt:[],      // signature: void
    _onDownloadProgress: [],    // signature: downloaded, total

    _totalDownloadBytes: 0,
    _archiveLocation: "split",

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
    process: function(descriptUrl) {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', descriptUrl);
        xhr.responseType = 'text';
        xhr.onload = function(evt) {
            Combine.onReceiveDescription(xhr);
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

        this._totalDownloadBytes = 0;
    },

    onReceiveDescription: function(xhr) {
        var json = JSON.parse(xhr.responseText);
        this._targets = json.content;
        this._totalDownloadBytes = 0;

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

    requestPiece: function(target, index) {
        if (index <  target.lastRequestedPiece) {
            throw "Request out of order";
        }

        target.lastRequestedPiece = index;
        target.progress = {};

        var item = target.pieces[index];
        var xhr = new XMLHttpRequest();
        xhr.open('GET', this._archiveLocation + '/' + item.name, true);
        xhr.responseType = 'arraybuffer';
        xhr.onprogress = function(evt) {
            if (evt.total && evt.lengthComputable) {
                target.progress[item.name] = {};
                target.progress[item.name].total = evt.total;
            }
            if (evt.loaded && evt.lengthComputable) {
                target.progress[item.name].downloaded = evt.loaded;
                Combine.updateProgress(target);
            }
        };
        xhr.onload = function(evt) {
            item.data = new Uint8Array(xhr.response);
            item.dataLength = item.data.length;
            Combine.copyData(target, item);
            Combine.onPieceLoaded(target, item);
            item.data = undefined;
        };
        xhr.send(null);
    },

    updateProgress: function(target) {
        var total_downloaded = 0;
        for (var p in target.progress) {
            total_downloaded += target.progress[p].downloaded;
        }
        for(i = 0; i<this._onDownloadProgress.length; ++i) {
            this._onDownloadProgress[i](total_downloaded, this._totalDownloadBytes);
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
        canvas.insertAdjacentHTML('afterend', '<div id="' + Progress.progress_id + '" class="canvas-app-progress"><div id="' + Progress.bar_id + '" class="canvas-app-progress-bar" style="width: 0%;">0%</div></div>');
        Progress.bar = document.getElementById(Progress.bar_id);
        Progress.progress = document.getElementById(Progress.progress_id);
    },

    updateProgress: function (percentage, text) {
        Progress.bar.style.width = percentage + "%";

        text = (typeof text === 'undefined') ? Math.round(percentage) + "%" : text;
        Progress.bar.innerText = text;
    },

    removeProgress: function () {
        Progress.progress.remove();
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

    print: function(text) { console.log(text); },
    printErr: function(text) { console.error(text); },

    setStatus: function(text) { console.log(text); },

    runApp: function(app_canvas_name, splash_image, archive_location) {
        app_canvas_name = (typeof app_canvas_name === 'undefined') ?  'canvas' : app_canvas_name;
        splash_image = (typeof splash_image === 'undefined') ?  'splash_image.png' : splash_image;
        archive_location = (typeof archive_location === 'undefined') ?  'split' : archive_location;

        Module.canvas = document.getElementById(app_canvas_name);
        Module.canvas.style.background = 'no-repeat center url("' + splash_image + '")';

        // Override game keys
        CanvasInput.addToCanvas(Module.canvas);

        // Load Facebook API
        var fb = document.createElement('script');
        fb.type = 'text/javascript';
        fb.src = '//connect.facebook.net/en_US/sdk.js';
        document.head.appendChild(fb);

        // Add progress visuals
        Progress.addProgress(Module.canvas);

        // Load and assemble archive
        Combine.addCombineCompletedListener(Module.onArchiveFileLoaded);
        Combine.addAllTargetsBuiltListener(Module.onArchiveLoaded);
        Combine.addProgressListener(Module.onArchiveLoadProgress);
        Combine._archiveLocation = archive_location;
        Combine.process(archive_location + '/archive_files.json');
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

        if(typeof Module.run == 'undefined') {
            /* If we are waiting for engine, let it autostart. */
            Module.noInitialRun = false;
        } else {
            Module.preloadAll();
            Progress.removeProgress();
            Module.callMain();
        }
    },

    preloadAll: function() {
        for (var i = 0; i < Module._filesToPreload.length; ++i) {
            var item = Module._filesToPreload[i];
            FS.createPreloadedFile("", item.path, item.data, true, true);
        }
    },

    preInit: [function() {
        /* Mount filesystem on preinit */
        var dir = DMSYS.GetUserPersistentDataRoot();
        FS.mkdir(dir);

        if (window.__indexedDB) {
            FS.mount(IDBFS, {}, dir);
            FS.syncfs(true, function(err) {
                if(err) {
                     throw "FS syncfs error: " + err;
                }
            });
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
};

window.onerror = function() {
    Module.setStatus('Exception thrown, see JavaScript console');
    Module.setStatus = function(text) {
        if (text) Module.printErr('[post-exception status] ' + text);
    };
};
