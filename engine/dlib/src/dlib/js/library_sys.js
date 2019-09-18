
/**
 *  Helper functions used in the process of setting up user persistent data in browsers.
 */
var LibraryDmSys = {
        $DMSYS: {
            _folder: '/data',
            _cstr: null,

            GetUserPersistentDataRoot : function() {
                if (typeof window !== 'undefined')
                    return DMSYS._folder;
                else
                    return '';
            },

            PumpMessageQueue: function() {
               if (typeof window === 'undefined') {
                   var uvrun = require('uvrun');
                   uvrun.runOnce();
               }
            }
        },

        /**
         * The mount point used with the IDBFS on browser, provided that we are running
         * in that environment.
         * @return the preferred mount point for the FS.
         */
        dmSysGetUserPersistentDataRoot: function() {
            if (null == DMSYS._cstr) {
                var str = DMSYS.GetUserPersistentDataRoot();
                DMSYS._cstr = _malloc(str.length + 1);
                Module.stringToUTF8(str, DMSYS._cstr, str.length + 1);
            }
            return DMSYS._cstr;
        },

        dmSysPumpMessageQueue: function() {
            DMSYS.PumpMessageQueue();
        },

        dmSysGetUserPreferredLanguage: function(defaultlang) {
            var jsdefault = Pointer_stringify(defaultlang);
            var preferred = navigator == undefined ? jsdefault : (navigator.languages ? navigator.languages[0] : (navigator.language || navigator.userLanguage || navigator.browserLanguage || navigator.systemLanguage || jsdefault) );
            var buffer = _malloc(preferred.length + 1);
            Module.stringToUTF8(preferred, buffer, preferred.length + 1);
            return buffer;
        },

        dmSysGetUserAgent: function() {
            var useragent = navigator.userAgent;
            var buffer = _malloc(useragent.length + 1);
            Module.stringToUTF8(useragent, buffer, useragent.length + 1);
            return buffer;
        },

        dmSysGetApplicationPath: function() {
            var path = location.href.substring(0, location.href.lastIndexOf("/"));
             // 'path.length' would return the length of the string as UTF-16 units, but Emscripten C strings operate as UTF-8.
            var lengthBytes = lengthBytesUTF8(path) + 1;
            var buffer = _malloc(lengthBytes);
            Module.stringToUTF8(path, buffer, lengthBytes);
            return buffer;
        },

        dmSysOpenURL__deps: ['$JSEvents'],
        dmSysOpenURL: function(url) {
            var jsurl = Pointer_stringify(url);
            if (window.open(jsurl) == null) {
                window.location = jsurl;
            }

            return true;
        }
};
autoAddDeps(LibraryDmSys, '$DMSYS');
mergeInto(LibraryManager.library, LibraryDmSys);
