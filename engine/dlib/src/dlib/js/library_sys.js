
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
        }
}
autoAddDeps(LibraryDmSys, '$DMSYS');
mergeInto(LibraryManager.library, LibraryDmSys);
