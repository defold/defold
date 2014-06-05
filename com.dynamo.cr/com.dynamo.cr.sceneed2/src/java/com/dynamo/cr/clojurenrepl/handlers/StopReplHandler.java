package com.dynamo.cr.clojurenrepl.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.services.ISourceProviderService;

import com.dynamo.cr.clojurenrepl.provider.ReplSessionProvider;

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
            IWorkbenchWindow window = HandlerUtil.getActiveWorkbenchWindow(event);
            ISourceProviderService service = (ISourceProviderService) window.getService(ISourceProviderService.class);
            ReplSessionProvider sessionSourceProvider = (ReplSessionProvider) service.getSourceProvider(ReplSessionProvider.REPL_STOPPER);
            sessionSourceProvider.getSessionActive().stop();
            return null;
        } catch (Exception e) {
            throw new ExecutionException("Error in event handler", e);
        }
    }
}
