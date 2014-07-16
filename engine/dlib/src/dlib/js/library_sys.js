
/**
 *  Helper functions used in the process of setting up user persistent data in browsers.
 */
var LibraryDmSys = {
        $DMSYS: { },

        /**
         * The mount point used with the IDBFS on browser, provided that we are running
         * in that environment.
         * @return the preferred mount point for the FS.
         */
        dmSysGetUserPersistentDataRoot: function()
        {
            if (typeof window !== 'undefined')
                return '/data';
            else
                return '';
        }
}
autoAddDeps(LibraryDmSys, '$DMSYS');
mergeInto(LibraryManager.library, LibraryDmSys);
