package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IWorkbenchPage;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.IWorkbenchPartSite;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.CommitPresenter;
import com.dynamo.cr.editor.MergePresenter;
import com.dynamo.cr.editor.dialogs.CommitDialog;
import com.dynamo.cr.editor.dialogs.MergeDialog;

public abstract class BasePublishUpdateHandler extends AbstractHandler {

    protected String title;
    protected Shell shell;
    protected IBranchClient branchClient;
    protected IProjectClient projectClient;

    enum Action {
        ABORT,
        RESTART,
        DONE,
    }

    Action commit() throws RepositoryException {
        CommitDialog dialog = new CommitDialog(shell);
        CommitPresenter presenter = new CommitPresenter(dialog);
        presenter.setBranchClient(branchClient);
        dialog.setPresenter(presenter);

        int ret = dialog.open();
        if (ret == Window.OK) {
            branchClient.commit(dialog.getCommitMessage());
            return Action.RESTART;
        }

        return Action.ABORT;
    }

    Action merge() throws RepositoryException {
        MergeDialog dialog = new MergeDialog(shell);
        MergePresenter prestener = new MergePresenter(dialog);
        prestener.setBranchResourceClient(branchClient);
        dialog.setPresenter(prestener);
        int ret = dialog.open();
        if (ret == Window.OK) {
            prestener.commit();
            return Action.RESTART;
        }
        return null;
    }

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {

        projectClient = Activator.getDefault().projectClient;
        branchClient = Activator.getDefault().getBranchClient();
        if (branchClient == null) {
            return null;
        }

        shell = HandlerUtil.getActiveShell(event);

        try {
            IWorkbenchPart part = HandlerUtil.getActivePart(event);
            boolean save_ret = part.getSite().getPage().saveAllEditors(true);
            if (!save_ret)
                return null;

            if (!preExecute())
                return null;

            IWorkbenchPartSite site = part.getSite();
            IWorkbenchPage page = site.getPage();
            save_ret = page.closeAllEditors(true);
            if (!save_ret)
                return null;

            boolean cont = true;
            while (cont) {
                Action action = start();
                if (action == null)
                    break;

                switch (action) {
                case ABORT:
                    cont = false;
                    break;

                case RESTART:
                    break;

                case DONE:
                    cont = false;
                    break;
                }
            }

        } catch (RepositoryException e) {
            MessageDialog.openError(shell, "Error", e.getMessage());
        }

        return null;
    }

    abstract boolean preExecute() throws RepositoryException;

    abstract Action start() throws RepositoryException;

}
