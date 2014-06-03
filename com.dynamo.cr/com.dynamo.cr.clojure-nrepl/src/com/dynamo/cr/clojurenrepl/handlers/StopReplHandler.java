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
	public Object execute(ExecutionEvent event) throws ExecutionException {
	  Activator.plugin.stopRepl();
	  
		return null;
	}
}
