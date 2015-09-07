
/*
 * Defold uses these three elements for such things as communicating status and handling the initial splash screen.
 */

var Module = {
    {{DEFOLD_STACK_SIZE}}

        'noInitialRun': true,
        'preRunTasks': [],
        'preRunTasksCompleted': 0,

        _pendingFiles: [],
        _totalFiles: 0,
        _mountedFiles: 0,
        _allPreloaded: false,

        'initialiseScript': function() {
            this['registerBootstrapTask'](Module.mountFilesystem);
            this['registerBootstrapTask'](Module.waitForArchive);
            Combine['addCombineCompletedListener'](Module.onFilePreloaded);
            Combine['addAllTargetsBuiltListener'](Module.onAllFilesPreloaded);
            Combine['addProgressListener'](Module.onPreloadProgress);
            Combine['process']('{{DEFOLD_SPLIT}}', Module.onPreloadDescriptionProcessed);
            this.setStatus('Downloading...');
        },

        onPreloadProgress: function(downloaded, total) {
            if (typeof SplashControl !== 'undefined' && typeof SplashControl.onSetProgress !== 'undefined') {
              SplashControl.onSetProgress(downloaded, total);
            }
        },

        onPreloadDescriptionProcessed: function(totalFiles) {
            Module._totalFiles = totalFiles;
        },

        onFilePreloaded: function(name, data) {
            Module.mountFile(name, data, true);
        },

        onAllFilesPreloaded: function() {
            Module._allPreloaded = true;
            var fsCreate = Module['FS_createPreloadedFile'];
            if (typeof fsCreate != 'undefined') {
                if (0 < Module._pendingFiles.length) {
                  Module.mountPendingFiles();
                } else {
                  Combine['cleanUp']();
                  Module.onBootstrapTaskCompleted();
                }
            }
            if (typeof SplashControl !== 'undefined' && typeof SplashControl.onDismissSplash) {
                SplashControl.onDismissSplash();
            }
        },

        mountFile: function(name, data, permitPending) {
            var fsCreate = Module['FS_createPreloadedFile'];
            if (typeof fsCreate != 'undefined') {
                fsCreate(name, null, data, true, true,
                    function() {
                        ++Module._mountedFiles;
                        if (Module._allPreloaded && Module._mountedFiles == Module._totalFiles) {
                            Combine['cleanUp']();
                            Module.onBootstrapTaskCompleted();
                        }
                    },
                    function() {
                         Module.printErr("FAILED TO MOUNT: " + name);
                    },
                    false, true);
            } else {
                if (permitPending) {
                    this._pendingFiles.push({path: name, data: data});
                } else {
                    throw "FS unavailable";
                }
            }
        },

        waitForArchive: function() {
            if (Module._totalFiles > Module._mountedFiles) {
                Module.mountPendingFiles();
            }
        },

        mountPendingFiles: function() {
          for (var i=0; i<Module._pendingFiles.length; ++i) {
            var item = Module._pendingFiles[i];
                Module.mountFile(item.path, item.data, false);
          }
          Module._pendingFiles = [];
        },

        'preRun': [function() {
            var bootstrap = Module['preRunTasks'];
            var i;
            for (i=0; i<bootstrap.length; ++i) {
                bootstrap[i]();
            }
        }],

        onBootstrapTaskCompleted: function() {
            ++Module['preRunTasksCompleted'];
            if (Module['preRunTasksCompleted'] == Module['preRunTasks'].length) {
                Module.attemptLaunch();
            }
        },

        attemptLaunch: function() {
            if (this.remainingDependencies == 0) {
              if ( Module['preRunTasksCompleted'] == Module['preRunTasks'].length) {
                    Module['callMain']();
                } else if (0 < Module['preRun'].length) {
                  // If all dependencies are resolved but the preRun tasks
                    // haven't executed
                  // then we should invoke run.
                    Module['run']();
                }
            }
        },

        'registerBootstrapTask': function(fn) {
            Module['preRunTasks'].push(fn);
        },

        hasIndexedDB : function() {
          var has = false;

          has |= typeof(window.indexedDB) !== 'undefined'
            || typeof(window.webkitIndexedDB) !== 'undefined'
            || typeof(window.mozIndexedDB) !== 'undefined'
            || typeof(window.msIndexedDB) !== 'undefined'
            || typeof(window.oIndexedDB) !== 'undefined';
          return has;
        },

        mountFilesystem: function() {
          var dir = DMSYS.GetUserPersistentDataRoot();
            FS.mkdir(dir);

            if (Module.hasIndexedDB()) {
                FS.mount(IDBFS, {}, dir);
                FS.syncfs(true, function(err) {
                  // This operation will fail if the user is running a private
                  // browsing session.
                  // Since the user is explicit about not wanting data to
                  // persist, there's nothing to do
                  // here other than roll over.
                  // assert(!err);
                  Module.onBootstrapTaskCompleted();
              });
            } else {
        Module.onBootstrapTaskCompleted();
            }

            // Sync IDBFS on fclose.
            if (Module.indexedDBAvailable && typeof _fsync != 'undefined') {
                var _old_fsync = _fsync;
                _fsync = function(fd) {
                    var ret = _old_fsync(fd);
                    if (ret == 0) {
                        FS.syncfs(false, function(err) { });
                    }
                    return ret;
                }
            }
        },

        postRun: [],

        print: function(text) { console.log(text); },
        printErr: function(text) { console.error(text); },

        canvas: document.getElementById('canvas'),
        setStatus: function(text) {
              if (!Module.setStatus.last) Module.setStatus.last = { time: Date.now(), text: '' };
              if (text === Module.setStatus.text) return;
              var m = text.match(/([^(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
              var now = Date.now();
              if (m && now - Module.setStatus.last.time < 30) return; // if this
                                                                        // is a
                                                                        // progress
                                                                        // update,
                                                                        // skip it
                                                                        // if too
                                                                        // soon
              if (m) {
                  text = m[1];
                  if (typeof SplashControl != 'undefined' && typeof SplashControl.onSetProgress != 'undefined') {
                      SplashControl.onSetProgress(parseInt(m[2])*100, parseInt(m[4])*100);
                  }
              }
              if (text && 0 < text.length && typeof SplashControl !== 'undefined' && typeof SplashControl.onSetMessage != 'undefined') {
                  SplashControl.onSetMessage(text);
              }
              Module.setStatus.last.time = Date.now();
        },
        totalDependencies: 0,
        remainingDependencies: 0,
        monitorRunDependencies: function(left) {
            this.totalDependencies = Math.max(this.totalDependencies, left);
            this.remainingDependencies = left;
            if (0 == left) {
                Module.attemptLaunch();
            }
            if (Module._allPreloaded) {
                Module.setStatus(left ? 'Preparing... (' + (this.totalDependencies-left) + '/' + this.totalDependencies + ')' : 'All downloads complete.');
            }
        },

        matchToCanvas: function(id) {
            if (typeof window != 'undefined') {
                var element = document.getElementById(id);
                var x = {{DEFOLD_DISPLAY_WIDTH}};
                var y = {{DEFOLD_DISPLAY_HEIGHT}};
                // The total screen size in device pixels in integers.
                var screenWidth = Math.round(window.innerWidth*window.devicePixelRatio);
                var screenHeight = Math.round(window.innerHeight*window.devicePixelRatio);
                // Compute (w,h) that will be the target CSS pixel size of the
                    // canvas.
                var w = screenWidth;
                var h = screenHeight;
                // Make sure aspect ratio remains after the resize
                if (w*y < x*h) {
                    h = (w * y / x) | 0;
                } else if (w*y > x*h) {
                    w = (h * x / y) | 0;
                }
                var topMargin = ((screenHeight - h) / 2) | 0;
                // Back to CSS pixels.
                topMargin /= window.devicePixelRatio;
                w /= window.devicePixelRatio;
                h /= window.devicePixelRatio;
                // Round to nearest 6 digits of precision.
                w = Math.round(w*1000000)/1000000;
                h = Math.round(h*1000000)/1000000;
                topMargin = Math.round(topMargin*1000000)/1000000;
                element.style.width = w + 'px';
                element.style.height = h + 'px';
                element.style.marginTop = topMargin + 'px';
            }
        },

        setMarginTop: function(id, sourcePixels) {
            if (typeof window != 'undefined') {
              var sourceHeight = {{DEFOLD_DISPLAY_HEIGHT}};
                  // The total screen size in device pixels in integers.
                  var screenHeight = Math.round(window.innerHeight*window.devicePixelRatio);
                  var scale = screenHeight / sourceHeight;
                  var element = document.getElementById(id);
                  element.style.marginTop = Math.round(sourcePixels * scale) + 'px';
               }
          }
      };
      Module['initialiseScript']();

      window.onerror = function() {
          Module.setStatus('Exception thrown, see JavaScript console');
          Module.setStatus = function(text) {
              if (text) Module.printErr('[post-exception status] ' + text);
      };
};
