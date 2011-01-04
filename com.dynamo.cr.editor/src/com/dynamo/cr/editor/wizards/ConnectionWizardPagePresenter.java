package com.dynamo.cr.editor.wizards;

import java.util.ArrayList;
import java.util.Collection;

import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.swt.widgets.Shell;

import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.protocol.proto.Protocol.BranchList;

public class ConnectionWizardPagePresenter {

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
    }

    private IDisplay display;
    private IProjectClient client;
    private boolean connectionOk;
    private String user;

    public ConnectionWizardPagePresenter(IDisplay display) {
        this.display = display;
    }

    public void onSelectBranch(String branch) {
        display.setDeleteBranchButtonEnabled(branch != null && connectionOk);
        display.setPageComplete(canFinish());
    }

    public void onNewBranch() {
        String name = display.openNewBranchDialog();
        if (name == null)
            return;

        try {
            client.createBranch(user, name);
            updateBranchList();
            display.setSelected(name);
            display.setPageComplete(canFinish());
        } catch (Throwable e) {
            display.setErrorMessage(e.getMessage());
        }
    }

    public void onDeleteBranch() {
        String branch = display.getSelectedBranch();
        if (branch == null)
            throw new RuntimeException("null branch");

        try {
            client.deleteBranch(user, branch);
            Activator.getDefault().disconnectFromBranch();
            updateBranchList();

        } catch (Throwable e) {
            display.setErrorMessage(e.getMessage());
        }
    }

    public void setProjectResourceClient(IProjectClient client, String user) {
        this.client = client;
        this.user = user;
    }

    void updateBranchList() throws RepositoryException  {
        BranchList branchList = client.getBranchList(user);
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
        try {
            updateBranchList();
        }
        catch (Throwable e) {
            connectionOk = false;
            display.setErrorMessage(e.getMessage());
        }
        finally {
            display.setNewBranchButtonEnabled(connectionOk);
        }
    }

    public boolean canFinish() {
        return display.getErrorMessage() == null && display.getSelectedBranch() != null;
    }

    public boolean finish() {
        try {
            Activator.getDefault().connectToBranch(user, display.getSelectedBranch());
        }
        catch (Throwable e) {
            MessageDialog.openError(display.getShell(), "Connection error", e.getMessage());
            e.printStackTrace();
            return false;
        }
        return true;
    }
}
