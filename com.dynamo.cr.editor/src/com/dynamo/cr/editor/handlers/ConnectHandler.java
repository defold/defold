package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.wizard.WizardDialog;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.wizards.ConnectionWizard;

public class ConnectHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        ConnectionWizard wiz = new ConnectionWizard();

        WizardDialog dialog = new WizardDialog(HandlerUtil.getActiveShell(event), wiz);
        dialog.open();

        return null;
    }
}
