
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
                Module.writeStringToMemory(str, DMSYS._cstr);
            }
            return DMSYS._cstr;
        },

        dmSysPumpMessageQueue: function() {
            DMSYS.PumpMessageQueue();
        },

        dmSysGetUserPreferredLanguage: function(defaultlang) {
            var jsdefault = Pointer_stringify(defaultlang);
            var preferred = navigator == undefined ? jsdefault : (navigator.languages ? navigator.languages[0] : (navigator.language || navigator.userLanguage || navigator.browserLanguage || navigator.systemLanguage || jsdefault) );
            var buffer = _malloc(preferred.length + 1);
            writeStringToMemory(preferred, buffer);
            return buffer;
        },

        dmSysGetUserAgent: function() {
            var useragent = navigator.userAgent;
            var buffer = _malloc(useragent.length + 1);
            writeStringToMemory(useragent, buffer);
            return buffer;
        }
}
autoAddDeps(LibraryDmSys, '$DMSYS');
mergeInto(LibraryManager.library, LibraryDmSys);
