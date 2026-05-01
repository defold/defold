var LibraryRenderer = {
    $dmRenderer: {
        renderContext: null,
        renderCallback: null,
        CONTEXT_LOST_EVENT: 0, // the same as dmRender::CONTEXT_LOST
        CONTEXT_RESTORED_EVENT: 1, // the same as dmRender::CONTEXT_RESTORED

        rendererContextEvent: function(event_type) {
            if (dmRenderer.renderCallback) {
                {{{ makeDynCall('vii', 'dmRenderer.renderCallback') }}}(dmRenderer.renderContext, event_type);
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
