package com.dynamo.cr.editor.util;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Platform;
import org.eclipse.jface.dialogs.ErrorDialog;
import org.eclipse.jface.operation.IRunnableWithProgress;
import org.eclipse.jface.preference.IPreferenceStore;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.progress.IProgressService;

import com.dynamo.cr.client.IProjectClient;
import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.preferences.PreferenceConstants;
import com.dynamo.cr.protocol.proto.Protocol.ApplicationInfo;

public class DownloadApplication {

    private static class CopyRunnable implements IRunnableWithProgress {

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

            monitor.beginTask("Downloading application", fileSize);
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

    public static boolean downloadApplication(Shell shell) {
        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        if (store.getBoolean(PreferenceConstants.P_DOWNLOAD_APPLICATION)) {
            IProjectClient projectClient = Activator.getDefault().projectClient;

            String platform = Activator.getPlatform();

            try {

                ApplicationInfo appInfo = projectClient.getApplicationInfo(platform);

                String application = store.getString(PreferenceConstants.P_APPLICATION);
                boolean download = true;
                if (!application.equals("")) {
                    File f = new File(application);
                    if (f.exists()) {
                        byte[] buffer = new byte[(int) f.length()];

                        MessageDigest md = null;
                        try {
                            md = MessageDigest.getInstance("SHA-1");
                        } catch (NoSuchAlgorithmException e) {
                            throw new RuntimeException(e);
                        }

                        FileInputStream is = new FileInputStream(f);
                        is.read(buffer);
                        md.update(buffer);
                        byte[] digest = md.digest();
                        StringBuffer hexDigest = new StringBuffer();
                        for (byte b : digest) {
                            hexDigest.append(Integer.toHexString(0xFF & b));
                        }

                        is.close();

                        String version = hexDigest.toString();

                        download = !version.equals(appInfo.getVersion());
                    }
                }

                if (download) {
                    String configPath = Platform.getConfigurationLocation().getURL().getPath();
                    String applicationPath = new Path(configPath).toPortableString();
                    new File(applicationPath).mkdirs();

                    String applicationFile = new Path(applicationPath).append(appInfo.getName()).toPortableString();

                    FileOutputStream os = null;
                    InputStream is = null;
                    try {
                        os = new FileOutputStream(applicationFile);
                        is = projectClient.getApplicationData(platform);

                        CopyRunnable copy = new CopyRunnable(is, os, appInfo.getSize());

                        IProgressService service = PlatformUI.getWorkbench().getProgressService();
                        service.run(true, false, copy);

                        if (copy.exception != null) {
                            ErrorDialog.openError(shell, "Error Downloading Application", copy.exception.getMessage(),
                                    new org.eclipse.core.runtime.Status(IStatus.ERROR, Activator.PLUGIN_ID, copy.exception.getMessage(), copy.exception));
                        } else {
                            store.setValue(PreferenceConstants.P_APPLICATION, applicationFile);

                            if (!platform.equals("win32")) {
                                Runtime.getRuntime().exec("chmod +x " + applicationFile);
                            }
                            return true;
                        }

                    } catch (Throwable e) {
                        ErrorDialog.openError(shell, "Error Downloading Application", e.getMessage(),
                                new org.eclipse.core.runtime.Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e));
                    }
                    finally {
                        try {
                            if (os != null)
                                os.close();
                            if (is != null)
                                is.close();
                        } catch (IOException e) {}
                    }
                }
            } catch (Exception e) {
                ErrorDialog.openError(shell, "Error Downloading Application", e.getMessage(),
                        new org.eclipse.core.runtime.Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e));
            }
        }

        return false;
    }

}
