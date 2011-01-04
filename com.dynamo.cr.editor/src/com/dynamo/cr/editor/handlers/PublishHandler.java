package com.dynamo.cr.editor.handlers;

import org.eclipse.jface.dialogs.MessageDialog;

import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;

public class PublishHandler extends BasePublishUpdateHandler {

    public PublishHandler() {
        title = "Publish";
    }

    Action start() throws RepositoryException {
        BranchStatus status;
        status = branchClient.getBranchStatus();

        switch (status.getBranchState()) {
            case CLEAN:
                if (status.getCommitsBehind() > 0) {
                    return update();
                }
                else {
                    return publish();
                }
                // no break needed here.

            case DIRTY:
                return commit();

            case MERGE:
                return merge();

             default:
                 assert false;
        }

        return Action.ABORT;
    }

    private Action update() throws RepositoryException {
        boolean ret = MessageDialog.openQuestion(shell, "Update", "You are behind main branch. In order to continue you must update your branch. Continue?");
        if (ret) {
            branchClient.update();
            return Action.RESTART;
        }
        else {
            return Action.ABORT;
        }
    }

    private Action publish() throws RepositoryException {
        branchClient.publish();
        MessageDialog.openInformation(shell, "Publish", "Your changes are now published.");
        return Action.DONE;
    }

    @Override
    public boolean isEnabled() {
        return Activator.getDefault().getBranchClient() != null;
    }

    @Override
    boolean preExecute() throws RepositoryException {
        BranchStatus status;
        status = branchClient.getBranchStatus();

        // Preconditions for Publish
        if (status.getCommitsAhead() == 0 && status.getBranchState() == BranchStatus.State.CLEAN) {
            MessageDialog.openInformation(shell, title, "Nothing to publish");
            return false;
        }
        return true;
    }
}
