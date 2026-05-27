// Installed during "install_ext"

var Module = null

if (typeof window === 'undefined') {
	// TODO: Remove global variables. Use the "module" pattern in js?
	console.log("node.js detected")

	Module = {
	  'print': console.log.bind(console),
	  'printErr': console.error.bind(console)
	};

	var node_fs = require('fs')
	var _created_files = {}

	function preload(name) {
		var str_name = UTF8ToString(name);
		if (!_created_files[str_name] && node_fs.existsSync(str_name)) {
			//console.log("loading file: %s", str_name);
			var a = str_name.split("/")
			var parent = a.slice(0,-1).join("/")
			if (parent != "") {
				FS.createPath("", parent, true, true);
			}
			// TODO: Use parent instead of analyzePath here?
			var pi = FS.analyzePath(UTF8ToString(name));

			var data = node_fs.readFileSync(UTF8ToString(name));

			var target = FS.createDataFile(pi.parentPath, pi.name, data, true, false);
			_created_files[str_name] = true;
		}
	}

	if (typeof _fopen != 'undefined') {
		_old_fopen = _fopen
		_fopen = function(name, mode) {
			preload(name);
			var ret = _old_fopen(name, mode);
			return ret;
		}
	}

	if (typeof _stat != 'undefined') {
		_old_stat = _stat
		_stat = function(path, buf) {
			preload(path);
			var r = _old_stat(path, buf);
			return r;
		}
	}

	try {
		XMLHttpRequest = require('xhr2');
		console.log("xhr2 for XMLHttpRequest loaded");
	} catch (err) {
		console.log("xhr2 not found");
	}
}
