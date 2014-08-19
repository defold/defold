if (!(typeof window === 'undefined')) {
    var Module;
    if (typeof Module === 'undefined') Module = eval('(function() { try { return Module || {} } catch(e) { return {} } })()');
    if (!Module.expectedDataFileDownloads) {
        Module.expectedDataFileDownloads = 0;
        Module.finishedDataFileDownloads = 0;
    }

    // .darc and .projectc files
    Module.expectedDataFileDownloads = 2;

    (function() {

        // Actual sizes are fetched below, via fetchFileSize. Order is important here: game.darc then game.projectc
        var PACKAGE_INFO = [
            {"name":'game.darc',     "remote_name":(Module['filePackagePrefixURL'] || '') + 'game.darc',     "size":0},
            {"name":'game.projectc', "remote_name":(Module['filePackagePrefixURL'] || '') + 'game.projectc', "size":0}
        ];

        var PACKAGE_PATH;
        if (typeof window === 'object') {
            PACKAGE_PATH = window['encodeURIComponent'](window.location.pathname.toString().substring(0, window.location.pathname.toString().lastIndexOf('/')) + '/');
        } else {
            // worker
            PACKAGE_PATH = encodeURIComponent(location.pathname.toString().substring(0, location.pathname.toString().lastIndexOf('/')) + '/');
        }

        /**
         * The fetched file size is used for download progress.
         **/
        function fetchFileSize(packageName, callback) {
            var xhr = new XMLHttpRequest();
            // Note: synchronous call is by design...
            xhr.open('HEAD', packageName, false);
            xhr.onload = function(event) {
                callback(parseInt(xhr.getResponseHeader("Content-Length")));
            };
            xhr.send(null);
        }

        for(var i=0; i<Module.expectedDataFileDownloads; i++) {
            fetchFileSize(PACKAGE_INFO[i].remote_name, function(fileSize) {PACKAGE_INFO[i].size = fileSize});
        }

        function fetchRemotePackage(packageName, packageSize, callback, errback) {
            var xhr = new XMLHttpRequest();
            xhr.open('GET', packageName, true);
            xhr.responseType = 'arraybuffer';
            xhr.onprogress = function(event) {
                var url = packageName;
                var size = packageSize;
                if (event.total) size = event.total;
                if (event.loaded) {
                    if (!xhr.addedTotal) {
                        xhr.addedTotal = true;
                        if (!Module.dataFileDownloads) Module.dataFileDownloads = {};
                        Module.dataFileDownloads[url] = {
                            loaded: event.loaded,
                            total: size
                        };
                    } else {
                        Module.dataFileDownloads[url].loaded = event.loaded;
                    }
                    var total = 0;
                    var loaded = 0;
                    var num = 0;
                    for (var download in Module.dataFileDownloads) {
                        var data = Module.dataFileDownloads[download];
                        total += data.total;
                        loaded += data.loaded;
                        num++;
                    }
                    total = Math.ceil(total * Module.expectedDataFileDownloads/num);
                    if (Module['setStatus']) Module['setStatus']('Downloading data... (' + loaded + '/' + total + ')');
                } else if (!Module.dataFileDownloads) {
                    if (Module['setStatus']) Module['setStatus']('Downloading data...');
                }
            };
            xhr.onload = function(event) {
                var packageData = xhr.response;
                callback(packageData);
            };
            xhr.send(null);
        };

        function handleError(error) {
            console.error('package error:', error);
        };

        var fetched = [], fetchedCallback = null, totalSize=0;
        fetchRemotePackage(PACKAGE_INFO[0].remote_name, PACKAGE_INFO[0].size, function(data) {
            Module.finishedDataFileDownloads++;
            if (fetchedCallback) {
                fetchedCallback(data, PACKAGE_INFO[0].name, 0, PACKAGE_INFO[0].size);
            } else {
                fetched.push({});
                fetched[0].data = data;
                fetched[0].name = PACKAGE_INFO[0].name;
                fetched[0].offset = 0;
                fetched[0].size = data.lengh; // TODO: Can we trust this?
            }
        }, handleError);
        totalSize += PACKAGE_INFO[0].size;
        fetchRemotePackage(PACKAGE_INFO[1].remote_name, PACKAGE_INFO[1].size, function(data) {
            Module.finishedDataFileDownloads++;
            if (fetchedCallback) {
                fetchedCallback(data, PACKAGE_INFO[1].name, PACKAGE_INFO[0].size, PACKAGE_INFO[0].size+PACKAGE_INFO[1].size);
            } else {
                fetched.push({});
                fetched[1].data = data;
                fetched[1].name = PACKAGE_INFO[1].name;
                fetched[1].offset = fetched[0].size;
                fetched[1].size = data.lengh; // TODO: Can we trust this?
            }
        }, handleError);
        totalSize += PACKAGE_INFO[1].size;

        function runWithFS() {
            function assert(check, msg) {
                if (!check) throw msg + new Error().stack;
            }

            function DataRequest(start, end, crunched, audio) {
                this.start = start;
                this.end = end;
                this.crunched = crunched;
                this.audio = audio;
            }
            DataRequest.prototype = {
                requests: {},
                open: function(mode, name) {
                    this.name = name;
                    this.requests[name] = this;
                    Module['addRunDependency']('fp ' + this.name);
                },
                send: function() {},
                onload: function() {
                    var byteArray = this.byteArray.subarray(this.start, this.end);
                    this.finish(byteArray);
                },
                finish: function(byteArray) {
                    var that = this;
                    Module['FS_createPreloadedFile'](this.name, null, byteArray, true, true, function() {
                        Module['removeRunDependency']('fp ' + that.name);
                    }, function() {
                        if (that.audio) {
                            Module['removeRunDependency']('fp ' + that.name); // workaround for chromium bug 124926 (still no audio with this, but at least we don't hang)
                        } else {
                            Module.printErr('Preloading file ' + that.name + ' failed');
                        }
                    }, false, true); // canOwn this data in the filesystem, it is a slide into the heap that will never change
                    this.requests[this.name] = null;
                },
            };

            for(var i=0; i<PACKAGE_INFO.length; i++) {
                var soff = 0;
                if(i>0)
                    soff = PACKAGE_INFO[i-1].size;
                new DataRequest(soff, soff+PACKAGE_INFO[i].size, 0, 0).open('GET', '/'+PACKAGE_INFO[i].name);
            }

            var all_ptr = Module['_malloc'](totalSize);
            DataRequest.prototype.byteArray = Module['HEAPU8'].subarray(all_ptr, all_ptr+totalSize);

            function processPackageData(arrayBuffer, pkgname, offset, size) {
                assert(arrayBuffer, 'Loading data file failed.');
                var byteArray = new Uint8Array(arrayBuffer);
                var curr;

                // copy the entire loaded file into a spot in the heap. Files will refer to slices in that. They cannot be freed though.
                Module['HEAPU8'].set(byteArray, all_ptr + offset);
                DataRequest.prototype.requests["/"+pkgname].onload();
                Module['removeRunDependency']('datafile_/'+pkgname);
            };

            Module['addRunDependency']('datafile_/'+PACKAGE_INFO[0].name);
            Module['addRunDependency']('datafile_/'+PACKAGE_INFO[1].name);

            if (!Module.preloadResults) Module.preloadResults = {};

            Module.preloadResults[PACKAGE_INFO[0].name] = {fromCache: false};
            Module.preloadResults[PACKAGE_INFO[1].name] = {fromCache: false};

            if (Module.finishedDataFileDownloads == Module.expectedDataFileDownloads) {
                processPackageData(fetched[0].data, fetched[0].name, fetched[0].offset, fetched[0].size);
                processPackageData(fetched[1].data, fetched[1].name, fetched[1].offset, fetched[1].size);
                fetched = [];
            } else {
                fetchedCallback = processPackageData;
            }

        }
        if (Module['calledRun']) {
            runWithFS();
        } else {
            if (!Module['preRun']) Module['preRun'] = [];
            Module["preRun"].push(runWithFS); // FS is not initialized yet, wait for it
        }
    })();
}
