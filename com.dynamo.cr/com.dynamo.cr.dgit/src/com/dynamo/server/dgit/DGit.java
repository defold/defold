package com.dynamo.server.dgit;

import java.io.File;
import java.io.IOException;
import java.net.URL;

import org.eclipse.core.runtime.FileLocator;
import org.eclipse.core.runtime.Plugin;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.common.util.Exec;


public class DGit extends Plugin {

    private static DGit plugin;

    public static DGit getDefault() {
        return plugin;
    }

    @Override
    public void start(BundleContext context) throws Exception {
        super.start(context);
        plugin = this;
        if (!getPlatform().contains("win32")) {
            File binDir = new File(getGitDir() + "/bin");
            File libExecDir = new File(getGitDir() + "/libexec/git-core");

            for (File d : new File[] {binDir, libExecDir}) {
                for (File f : d.listFiles()) {
                    Exec.exec("chmod", "+x", f.getPath());
                }
            }
        }
    }

    @Override
    public void stop(BundleContext context) throws Exception {
        super.stop(context);
        plugin = null;
    }

    public String getGitDir() {
        String platform = DGit.getPlatform();
        URL bundleUrl = getBundle().getEntry("/git/" + platform);
        URL fileUrl;
        try {
            fileUrl = FileLocator.toFileURL(bundleUrl);
            return fileUrl.getPath();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public static File getNetRC() {
        String userHome = System.getProperty("user.home");
        File defoldDir = new File(userHome, ".defold");
        if (!defoldDir.exists()) {
            defoldDir.mkdir();
        }
        File netRC;
        if (getPlatform().contains("win32")) {
            netRC = new File(defoldDir, "_netrc");
        } else {
            netRC = new File(defoldDir, ".netrc");
        }
        return netRC;
    }

    public static String getPlatform() {
        String os_name = System.getProperty("os.name").toLowerCase();
        String arch = System.getProperty("os.arch").toLowerCase();

        if (os_name.indexOf("win") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return "x86_64-win32";
            } else {
                return "win32";
            }
        } else if (os_name.indexOf("mac") != -1) {
            return "darwin";
        } else if (os_name.indexOf("linux") != -1) {
            if (arch.equals("x86_64") || arch.equals("amd64")) {
                return "x86_64-linux";
            } else {
                return "linux";
            }
        }

        return null;
    }
}
