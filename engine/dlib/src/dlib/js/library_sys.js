
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
                DMSYS._cstr = stringToNewUTF8(str);
            }
            return DMSYS._cstr;
        },

        dmSysPumpMessageQueue: function() {
            DMSYS.PumpMessageQueue();
        },

        dmSysGetUserPreferredLanguage: function(defaultlang) {
            var jsdefault = UTF8ToString(defaultlang);
            var preferred = navigator == undefined ? jsdefault : (navigator.languages ? (navigator.languages[0] || jsdefault) : (navigator.language || navigator.userLanguage || navigator.browserLanguage || navigator.systemLanguage || jsdefault) );
            var buffer = stringToNewUTF8(preferred);
            return buffer;
        },

        dmSysGetUserAgent: function() {
            var useragent = navigator.userAgent;
            var buffer = stringToNewUTF8(useragent);
            return buffer;
        },

        dmSysGetApplicationPath: function() {
            var path = location.href.substring(0, location.href.lastIndexOf("/"));
            var buffer = stringToNewUTF8(path);
            return buffer;
        },

        dmSysOpenURL__deps: ['$JSEvents'],
        dmSysOpenURL: function(url, target) {
            var jsurl = UTF8ToString(url);
            var jstarget = UTF8ToString(target);
            if (jstarget == 0)
            {
                jstarget = "_self";
            }
            return window.open(jsurl, jstarget) != null;
        }
};
autoAddDeps(LibraryDmSys, '$DMSYS');
addToLibrary(LibraryDmSys);
