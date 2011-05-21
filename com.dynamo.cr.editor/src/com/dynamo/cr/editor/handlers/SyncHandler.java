package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.dialogs.SyncDialog;

public class SyncHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        SyncDialog syncDialog = new SyncDialog(HandlerUtil.getActiveShell(event));
        syncDialog.create();
        syncDialog.getShell().setSize(800, 600);
        int result = syncDialog.open();
        if (result == 0) {
            // revert
        }
        return null;
    }

    @Override
    public boolean isEnabled() {
        return Activator.getDefault().getBranchClient() != null;
    }
}
