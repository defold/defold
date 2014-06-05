package com.dynamo.cr.clojurenrepl.provider;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.ui.AbstractSourceProvider;
import org.eclipse.ui.ISources;

public class ReplSessionProvider extends AbstractSourceProvider {
    public final static String REPL_STOPPER = "com.dynamo.cr.clojurenrepl.replStopper";

    private IStop stopthunk = null;

    public ReplSessionProvider() {
    }

    @Override
    public void dispose() {
    }

    @SuppressWarnings("rawtypes")
    @Override
    public Map getCurrentState() {
        Map<String, Object> currentState = new HashMap<String, Object>(1);
        currentState.put(REPL_STOPPER, stopthunk);
        return currentState;
    }

    @Override
    public String[] getProvidedSourceNames() {
        return new String[] { REPL_STOPPER };
    }

    public IStop getSessionActive() {
        return this.stopthunk;
    }

    public void setReplStopper(IStop stopthunk) {
        if (this.stopthunk == stopthunk)
            return;
        this.stopthunk = stopthunk;
        fireSourceChanged(ISources.WORKBENCH, REPL_STOPPER, stopthunk);
    }
}
