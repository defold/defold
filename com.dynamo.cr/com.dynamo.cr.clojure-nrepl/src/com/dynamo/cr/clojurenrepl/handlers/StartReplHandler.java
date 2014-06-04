package com.dynamo.cr.clojurenrepl.handlers;

import org.clojure.tools.nrepl.Activator;
import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;

/**
 * Start a REPL, if one is not already running.
 * 
 * @see org.eclipse.core.commands.IHandler
 * @see org.eclipse.core.commands.AbstractHandler
 */
public class StartReplHandler extends AbstractHandler {
    /**
     * The constructor.
     */
    public StartReplHandler() {
    }

    /**
     * the command has been executed, so extract extract the needed information
     * from the application context.
     * 
     * @throws ExecutionException
     */
    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        try {
            Activator.plugin.startRepl();
        } catch (Exception e) {
            throw new ExecutionException("Error in event handler", e);
        }
        return null;
    }
}
