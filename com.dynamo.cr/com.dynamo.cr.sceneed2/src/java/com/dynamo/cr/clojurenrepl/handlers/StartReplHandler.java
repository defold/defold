package com.dynamo.cr.clojurenrepl.handlers;

import org.clojure.tools.nrepl.internal.ReplWrapper;
import org.clojure.tools.nrepl.internal.ReplWrapper.StopThunk;
import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.services.ISourceProviderService;
import org.osgi.framework.FrameworkUtil;

import com.dynamo.cr.clojurenrepl.provider.ReplSessionProvider;

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
            StopThunk stopthunk = new ReplWrapper().start(FrameworkUtil.getBundle(this.getClass()));

            IWorkbenchWindow window = HandlerUtil.getActiveWorkbenchWindow(event);
            ISourceProviderService service = (ISourceProviderService) window.getService(ISourceProviderService.class);
            ReplSessionProvider sessionSourceProvider = (ReplSessionProvider) service.getSourceProvider(ReplSessionProvider.REPL_STOPPER);
            sessionSourceProvider.setReplStopper(stopthunk);
        } catch (Exception e) {
            throw new ExecutionException("Error in event handler", e);
        }
        return null;
    }
}
