package com.dynamo.cr.editor.handlers;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.jface.dialogs.ProgressMonitorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.preference.IPreferenceStore;

import com.dynamo.cr.client.RepositoryException;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.protocol.proto.Protocol.ApplicationInfo;
import com.dynamo.cr.protocol.proto.Protocol.BranchStatus;

public class UpdateHandler extends BasePublishUpdateHandler {

    public UpdateHandler() {
        title = "Update";
    }

    Action start() throws RepositoryException {
        BranchStatus status;
        status = branchClient.getBranchStatus();

        switch (status.getBranchState()) {
            case CLEAN:
                if (status.getCommitsBehind() == 0) {
                    MessageDialog.openInformation(shell, title, "Update completed");
                    return Action.DONE;
                }
                return update();

            case DIRTY:
                return commit();

            case MERGE:
                return merge();

             default:
                 assert false;
        }

        return Action.ABORT;
    }

    Action update() throws RepositoryException {
        branchClient.update();
        return Action.RESTART;
    }

    @Override
    public boolean isEnabled() {
        return Activator.getDefault().getBranchClient() != null;
    }

    static class CopyRunnable implements IRunnableWithProgress {

        private InputStream inputStream;
        private FileOutputStream outputStream;
        private int fileSize;
        IOException exception;

        public CopyRunnable(InputStream is, FileOutputStream os, int file_size) {
            this.inputStream = is;
            this.outputStream = os;
            this.fileSize = file_size;
        }

        @Override
        public void run(IProgressMonitor monitor)
                throws InvocationTargetException, InterruptedException {

            monitor.beginTask("Downloading executable", fileSize);
            byte[] buffer = new byte[1024 * 64];

            try {
                int n = inputStream.read(buffer);
                while (n > 0) {
                    monitor.worked(n);
                    outputStream.write(buffer, 0, n);
                    n = inputStream.read(buffer);
                }
            }
            catch (IOException e) {
                this.exception = e;
            }
        }
    }

    boolean downloadApplication() throws RepositoryException {
        String platform = Activator.getPlatform();

        ApplicationInfo app_info = projectClient.getApplicationInfo(platform);

        String branch_path = branchClient.getURI().getPath();
        String config_path = Platform.getConfigurationLocation().getURL().getPath();
        String application_path = new Path(config_path).append(branch_path).toPortableString();
        new File(application_path).mkdirs();

        String application_file = new Path(application_path).append(app_info.getName()).toPortableString();

        FileOutputStream os = null;
        InputStream is = null;
        ProgressMonitorDialog monitor = new ProgressMonitorDialog(shell);
        try {
            os = new FileOutputStream(application_file);
            is = projectClient.getApplicationData(platform);

            CopyRunnable copy = new CopyRunnable(is, os, app_info.getSize());
            monitor.run(true, false, copy);

            if (copy.exception != null) {
                ErrorDialog.openError(shell, "Error downloading application", copy.exception.getMessage(),
                        new Status(IStatus.ERROR, Activator.PLUGIN_ID, copy.exception.getMessage(), copy.exception));
            }
            else {
                IPreferenceStore store = Activator.getDefault().getPreferenceStore();
                store.setValue(PreferenceConstants.P_APPLICATION, application_file);

                if (!platform.equals("win32")) {
                    Runtime.getRuntime().exec("chmod +x " + application_file);
                }
            }

        } catch (Throwable e) {
            ErrorDialog.openError(shell, "Error downloading application", e.getMessage(),
                    new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e));
            return false;
        }
        finally {
            try {
                if (os != null)
                    os.close();
                if (is != null)
                    is.close();
                monitor.close();
            } catch (IOException e) {}
        }

        return true;
    }

    @Override
    boolean preExecute() throws RepositoryException {
        BranchStatus status;
        status = branchClient.getBranchStatus();

        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        if (store.getBoolean(PreferenceConstants.P_DOWNLOADAPPLICATION)) {
            if (!downloadApplication())
                return false;
        }

        // Precondition for Update
        if (status.getCommitsBehind() == 0) {
            MessageDialog.openInformation(shell, title, "Nothing to update");
            return false;
        }
        return true;
    }

}
