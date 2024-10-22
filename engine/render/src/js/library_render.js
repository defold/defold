var LibraryRenderer = {
    $dmRenderer: {
        renderContext: null,
        renderCallback: null,

        rendererContextEvent: function(event_name) {
            if (dmRenderer.renderCallback) {
                var event_id = stringToUTF8OnStack(event_name);
                {{{ makeDynCall('vii', 'dmRenderer.renderCallback') }}}(dmRenderer.renderContext, event_id);
            }
        }
    },
    setupCallbackJS: function(context, callback) {
        dmRenderer.renderContext = context;
        dmRenderer.renderCallback = callback;
    }
}

autoAddDeps(LibraryRenderer, '$dmRenderer');
addToLibrary(LibraryRenderer);