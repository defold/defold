package com.dynamo.cr.editor;

import com.dynamo.cr.client.IBranchClient;
import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;

public class CommitPresenter {

    public interface IDisplay {
        void setBranchStatus(BranchStatus status);
        void setPageComplete(boolean completed);
        void setMessage(String message);
        void showExceptionDialog(Throwable e);
        void setCommitMessageEnabled(boolean enabled);
        void setErrorMessage(String message);
    }

    private IDisplay display;
    private Object originalState;
    private IBranchClient client;

    public CommitPresenter(IDisplay display) {
        this.display = display;
    }

    public void setBranchClient(IBranchClient client) {
        this.client = client;
    }

    public void init() {
        display.setPageComplete(false);
        try {
            BranchStatus status = client.getBranchStatus();
            originalState = status.getBranchState();
            if (originalState == BranchStatus.State.CLEAN) {
                display.setMessage("Nothing to commit.");
                display.setCommitMessageEnabled(false);
                display.setPageComplete(true);
            }
            else if (originalState == BranchStatus.State.DIRTY) {
                display.setBranchStatus(status);
            }
            else if (originalState == BranchStatus.State.MERGE) {
                display.setErrorMessage("Branch in merge state. You need to resolved first.");
                display.setCommitMessageEnabled(false);
                display.setPageComplete(true);
            }
            else {
                throw new RuntimeException("Invalid branch state: " + originalState);
            }
        } catch (RepositoryException e) {
            display.showExceptionDialog(e);
        }
    }

    public void onCommitMessage(String message) {
        if (originalState != BranchStatus.State.DIRTY) {
            return;
        }

        if (message.trim().isEmpty()) {
            display.setPageComplete(false);
        }
        else {
            display.setPageComplete(true);
        }

    }

}
