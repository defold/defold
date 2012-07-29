package com.dynamo.cr.editor.wizards;

import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.util.Collection;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.IWorkbenchPage;

import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;

public class ConnectionWizardBranchPagePresenter {

    public interface IDisplay {
        String getSelectedBranch();
        void setMessage(String message);
        void setErrorMessage(String message);
        String getErrorMessage();

        void setNewBranchButtonEnabled(boolean enabled);
        void setDeleteBranchButtonEnabled(boolean enabled);
        String openNewBranchDialog();
        boolean isNewBranchButtonEnabled();
        boolean isDeleteBranchButtonEnabled();
        //Collection<String> getBranchNames();
        void setBranchNames(Collection<String> branchNames);
        String[] getBranchNames();
        void setPageComplete(boolean pageComplete);
        boolean isPageComplete();
        void setSelected(String name);
        Shell getShell();
        void runWithProgress(IRunnableWithProgress runnable);
    }

    private IDisplay display;
    private IProjectClient client;
    private boolean connectionOk;
    private IWorkbenchPage page;

    public ConnectionWizardBranchPagePresenter(IDisplay display, IWorkbenchPage page) {
        this.display = display;
        this.page = page;
    }

    public void onSelectBranch(String branch) {
        display.setDeleteBranchButtonEnabled(branch != null && connectionOk);
        display.setPageComplete(canFinish());
    }

    private class CreateBranchRunnable implements IRunnableWithProgress {

        String name;
        RepositoryException exception;

        public CreateBranchRunnable(String name) {
            this.name = name;
        }

        @Override
        public void run(IProgressMonitor monitor)
                throws InvocationTargetException, InterruptedException {
            monitor.beginTask("Downloading data...", IProgressMonitor.UNKNOWN);
            try {
                client.createBranch(name);
            } catch (RepositoryException e) {
                this.exception = e;
            }
        }
    }

    public void onNewBranch() {
        String name = display.openNewBranchDialog();
        if (name == null)
            return;

        try {
            CreateBranchRunnable createBranchRunnable = new CreateBranchRunnable(name);
            display.runWithProgress(createBranchRunnable);
            if (createBranchRunnable.exception != null) {
                Activator.logException(createBranchRunnable.exception);
                display.setErrorMessage(createBranchRunnable.exception.getMessage());
            } else {
                updateBranchList();
                display.setSelected(name);
                display.setPageComplete(canFinish());
            }
        } catch (Throwable e) {
            Activator.logException(e);
            display.setErrorMessage(e.getMessage());
        }
    }

    public void onDeleteBranch() {
        String branch = display.getSelectedBranch();
        if (branch == null)
            throw new RuntimeException("null branch");

        try {
            client.deleteBranch(branch);
            Activator.getDefault().disconnectFromBranch();
            updateBranchList();

        } catch (Throwable e) {
            Activator.logException(e);
            if (e.getMessage() == null)
                display.setErrorMessage(e.toString());
            else
                display.setErrorMessage(e.getMessage());
        }
    }

    public void setProjectResourceClient(IProjectClient client) {
        this.client = client;
        try {
            display.setErrorMessage(null);
            updateBranchList();
        } catch (RepositoryException e) {
            display.setErrorMessage(e.getMessage());
            Activator.logException(e);
        }
    }

    void updateBranchList() throws RepositoryException  {
        // Clear list first in case of any exceptions
        display.setBranchNames(new ArrayList<String>());

        BranchList branchList = client.getBranchList();
        ArrayList<String> branchNames = new ArrayList<String>();
        for (String b : branchList.getBranchesList()) {
            branchNames.add(b);
        }
        display.setBranchNames(branchNames);
    }

    public void init() {
        display.setPageComplete(false);
        display.setDeleteBranchButtonEnabled(false);
        connectionOk = true;
        display.setNewBranchButtonEnabled(connectionOk);
    }

    public boolean canFinish() {
        return display.getErrorMessage() == null && display.getSelectedBranch() != null;
    }

    public boolean finish() {
        try {
            if (!this.page.closeAllEditors(true)) {
                return false;
            }

            Activator.getDefault().disconnectFromBranch();
            Activator.getDefault().connectToBranch(client, display.getSelectedBranch());
        }
        catch (Throwable e) {
            Activator.logException(e);
            MessageDialog.openError(display.getShell(), "Connection error", e.getMessage());
            return false;
        }
        return true;
    }
}
