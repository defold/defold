
/*
 *	Utility used to load and concatenate sets of files.
 *
 */
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

        'isCompleted': false,       // status of process

        _onCombineCompleted: [],    // signature: name, data.
        _onAllTargetsBuilt:[],      // signature: void
        _onDownloadProgress: [],    // signature: downloaded, total

        _totalDownloadBytes: 0,
        _currentDownloadBytes: 0,

        'addProgressListener': function(callback) {
            if (typeof callback !== 'function') {
                throw "Invalid callback registration";
            };
            this._onDownloadProgress.push(callback);
        },

        'addCombineCompletedListener': function(callback) {
            if (typeof callback !== 'function') {
                throw "Invalid callback registration";
            };
            this._onCombineCompleted.push(callback);
        },

        'addAllTargetsBuiltListener': function(callback) {
            if (typeof callback !== 'function') {
                throw "Invalid callback registration";
            };
            this._onAllTargetsBuilt.push(callback);
        },

        // descriptUrl: location of text file describing files to be preloaded
        // onDescriptionProcessed: callback, receiving the total number of files that will be preloaded
        'process': function(descriptUrl, onDescriptionProcessed) {
            var xhr = new XMLHttpRequest();
            xhr.open('GET', descriptUrl);
            xhr.responseType = 'text';
            xhr.onload = function(evt) {
                Combine.onReceiveDescription(xhr, onDescriptionProcessed);
            };
            xhr.send(null);
        },

        'cleanUp': function() {
            this._targets =  [];
            this._targetIndex = 0;
            this['isCompleted'] = false;
            this._onCombineCompleted = [];
            this._onAllTargetsBuilt = [];
            this._onDownloadProgress = [];

            this._totalDownloadBytes = 0;
            this._currentDownloadBytes = 0;
        },

        onReceiveDescription: function(xhr, onDescriptionProcessed) {
        	var json = JSON.parse(xhr.responseText);
        	this._targets = json.content;
        	this._totalDownloadBytes = 0;

            var targets = this._targets;
            for(var i=0; i<targets.length; ++i) {
                this._totalDownloadBytes += targets[i].size;
            }
            this.requestContent();
            if (onDescriptionProcessed) {
                onDescriptionProcessed(this._targets.length);
            }
        },

        requestContent: function() {
            var target = this._targets[this._targetIndex];
            if (1 < target.pieces.length) {
                target.data = new Uint8Array(target.size);
            }
            console.log("Produce target: " + target.name + ", " + target.size);
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

            var item = target.pieces[index];
            var xhr = new XMLHttpRequest();
            xhr.open('GET', item.name, true);
            xhr.responseType = 'arraybuffer';
            xhr.onprogress = function(evt) {
                var size = 0;
                if (evt.total) {
                    size = evt.total;
                }
                if (evt.loaded) {
                    target.downloaded = evt.loaded;
                    Combine.updateProgress(target);
                }
            };
            xhr.onload = function(evt) {
                item.data = new Uint8Array(xhr.response);
                item.dataLength = item.data.length;
                Combine._currentDownloadBytes += item.data.length;
                Combine.copyData(target, item);
                Combine.onPieceLoaded(target, item);
                item.data = undefined;
            };
            xhr.send(null);
        },

        updateProgress: function(target) {
            for(var i=0; i<this._onDownloadProgress.length; ++i) {
                this._onDownloadProgress[i](this._currentDownloadBytes + target.downloaded, this._totalDownloadBytes);
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
                this.finaliseTarget(target);
                ++this._targetIndex;
                for (var i=0; i<this._onCombineCompleted.length; ++i) {
                    this._onCombineCompleted[i](target.name, target.data);
                }
                if (this._targetIndex < this._targets.length) {
                    this.requestContent();
                } else {
                    Combine['isCompleted'] = true;
                    for (var i=0; i<this._onAllTargetsBuilt.length; ++i) {
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

        finaliseTarget: function(target) {
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
                for (var i=0; i<pieces.length; ++i) {
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

}
