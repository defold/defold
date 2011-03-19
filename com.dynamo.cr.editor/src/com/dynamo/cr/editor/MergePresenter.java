package com.dynamo.cr.editor;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;
import com.dynamo.cr.protocol.proto.Protocol.ResolveStage;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.State;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus.Status;

public class MergePresenter {

    private boolean finished = false;

    public interface IDisplay {
        void setBranchStatus(BranchStatus status);
        void setPageComplete(boolean completed);
        void setErrorMessage(String message);
        void setMessage(String message);
        void showExceptionDialog(Throwable e);
    }

    private IBranchClient client;
    private IDisplay display;

    public MergePresenter(IDisplay display) {
        this.display = display;
    }

    public void init() {
        display.setMessage("Select my or their version for every conflicting file below.");

        boolean b = update();
        if (b) {
            try {
                BranchStatus status = client.getBranchStatus();
                if (status.getBranchState() == State.CLEAN) {
                    display.setMessage("Nothing to merge");
                }
            } catch (RepositoryException e) {
                display.showExceptionDialog(e);
            }
        }
    }

    public boolean update() {
        boolean ret = true;
        BranchStatus status;
        try {
            status = client.getBranchStatus();
            display.setBranchStatus(status);

            if (status.getBranchState() == State.DIRTY) {
                display.setErrorMessage("Can't merge dirty branch. Publish your changes first.");
                return false;
            }

            finished = status.getBranchState() == State.CLEAN;
            // or all files are resolved
            if (status.getBranchState() == State.MERGE) {
                int n_unresolved = 0;
                for (Status x : status.getFileStatusList()) {
                    n_unresolved += x.getIndexStatus().equals("U") ? 1 : 0;
                }
                finished = n_unresolved == 0;
                if (finished) {
                    display.setMessage("Merge complete");
                }
            }

        } catch (RepositoryException e) {
            display.showExceptionDialog(e);
            ret = false;
        }
        finally {
            display.setPageComplete(canFinish());
        }

        return ret;
    }

    public void setBranchResourceClient(IBranchClient client) {
        this.client = client;
    }

    public boolean canFinish() {
        return finished;
    }

    public void resolve(String file, ResolveStage stage) {
        try {
            client.resolve(file, stage.toString().toLowerCase());
            update();

        } catch (RepositoryException e) {
            display.showExceptionDialog(e);
        }
    }

    public void commit() throws RepositoryException {
        client.commitMerge("Merge commit"); // TODO: Merge commit message..?
        /*
        try {
            client.commitMerge("Merge commit"); // TODO: Merge commit message..?
        } catch (RepositoryException e) {
            display.showExceptionDialog(e);
        }*/
    }
}
