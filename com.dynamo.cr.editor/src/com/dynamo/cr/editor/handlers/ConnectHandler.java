package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.wizard.WizardDialog;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.wizards.ConnectionWizard;

public class ConnectHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {

        // Ensure that we are authenticated. connectProjectClient() show sign-in
        // web page and any errors.
        boolean ret = Activator.getDefault().connectProjectClient();
        if (!ret) {
            return null;
        }

        // Fetch page straight from activator since getting part from event (via HandlerUtil)
        // does not work for win/linux if the openid-dialog just was shown
        IWorkbenchPage page = Activator.getDefault().getWorkbench().getActiveWorkbenchWindow().getActivePage();
        ConnectionWizard wiz = new ConnectionWizard(page);

        WizardDialog dialog = new WizardDialog(HandlerUtil.getActiveShell(event), wiz);
        dialog.open();

        return null;
    }
}
