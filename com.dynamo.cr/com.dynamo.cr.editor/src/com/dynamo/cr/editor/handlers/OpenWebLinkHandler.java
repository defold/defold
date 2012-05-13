package com.dynamo.cr.editor.handlers;

import java.net.URL;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.IHandler;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.browser.IWorkbenchBrowserSupport;

import com.dynamo.cr.editor.Activator;

public class OpenWebLinkHandler extends AbstractHandler implements IHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        String url = event.getParameter("com.dynamo.crepo.commands.openWebLink.url");
        if (url != null) {
            IWorkbenchBrowserSupport browserSupport = PlatformUI.getWorkbench().getBrowserSupport();
            try {
                browserSupport.getExternalBrowser().openURL(new URL(url));
            } catch (Exception e) {
                Activator.logException(e);
            }
        }
        return null;
    }
}
