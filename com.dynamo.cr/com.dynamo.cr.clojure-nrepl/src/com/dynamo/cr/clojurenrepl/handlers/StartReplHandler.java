package com.dynamo.cr.clojurenrepl.handlers;

import org.clojure.tools.nrepl.Activator;
import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;

import com.dynamo.cr.clojure_eclipse.ClojureEclipse;

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
	 */
	public Object execute(ExecutionEvent event) {
	  ClojureEclipse.getTracer().trace("log/info", "Event: " + event);
    ClojureEclipse.getTracer().trace("log/info", "Event.ApplicationContext: " + event.getApplicationContext());
    
    Activator.plugin.startRepl();
    
		return null;
	}
}
