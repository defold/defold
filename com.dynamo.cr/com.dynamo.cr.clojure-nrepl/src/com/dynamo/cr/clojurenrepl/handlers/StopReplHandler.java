package com.dynamo.cr.clojurenrepl.handlers;

import org.clojure.tools.nrepl.Activator;
import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;

/**
 * Stop a REPL, if one is running.
 * 
 * @see org.eclipse.core.commands.IHandler
 * @see org.eclipse.core.commands.AbstractHandler
 */
public class StopReplHandler extends AbstractHandler {
    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        try {
            Activator.plugin.stopRepl();
            return null;
        } catch (Exception e) {
            throw new ExecutionException("Error in event handler", e);
        }
    }
}
