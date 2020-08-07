package com.dynamo.bob.bundle;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

import java.util.logging.Level;
import java.util.logging.Logger;
import java.io.File;
import java.io.FileInputStream;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.ArrayList;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Project;
import com.dynamo.bob.Platform;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;

import java.io.IOException;
import com.dynamo.bob.CompileExceptionError;

public class AndroidAPK {
    private static Logger logger = Logger.getLogger(AndroidAPK.class.getName());

    private static void log(String s) { logger.log(Level.INFO, s); }

    private static Result exec(List<String> args) throws IOException {
		log("exec: " + String.join(" ", args));
		Map<String, String> env = new HashMap<String, String>();
		if (Platform.getHostPlatform() == Platform.X86_64Linux || Platform.getHostPlatform() == Platform.X86Linux) {
			env.put("LD_LIBRARY_PATH", Bob.getPath(String.format("%s/lib", Platform.getHostPlatform().getPair())));
		}
		return Exec.execResultWithEnvironment(env, args);
	}

    private static File createDir(File parent, String child) throws IOException {
		File dir = new File(parent, child);
		if (dir.exists()) {
			FileUtils.deleteDirectory(dir);
		}
		dir.mkdirs();
		return dir;
	}
	private static File createDir(String parent, String child) throws IOException {
		return createDir(new File(parent), child);
	}


    public static File createUniversalApks(File aab, File bundleDir, ICanceled canceled) throws CompileExceptionError {
        log("Creating universal APK Set");
        try {
            File bundletool = new File(Bob.getLibExecPath("bundletool-all.jar"));

            String aabPath = aab.getAbsolutePath();

            String name = FilenameUtils.getBaseName(aabPath);
            String apksPath = bundleDir.getAbsolutePath() + File.separator + name + ".apks";

            List<String> args = new ArrayList<String>();
            args.add("java"); args.add("-jar");
            args.add(bundletool.getAbsolutePath());
            args.add("build-apks");
            args.add("--mode"); args.add("universal");
            args.add("--bundle"); args.add(aabPath);
            args.add("--output"); args.add(apksPath);

            Result res = exec(args);
            if (res.ret != 0) {
                String msg = new String(res.stdOutErr);
                throw new IOException(msg);
            }
            BundleHelper.throwIfCanceled(canceled);
            return new File(apksPath);
        } catch (Exception e) {
            throw new CompileExceptionError(null, -1, "Failed building Android Application Bundle: " + e.getMessage());
        }
    }


    private static File extractUniversalApk(File apks, File bundleDir) throws IOException {
        log("Extracting universal APK from APK set");
        File apksDir = createDir(bundleDir, "apks");
        BundleHelper.unzip(new FileInputStream(apks), apksDir.toPath());

        File universalApk = new File(apksDir.getAbsolutePath() + File.separator + "universal.apk");
        File apk = new File(bundleDir.getAbsolutePath() + File.separator + FilenameUtils.getBaseName(apks.getPath()) + ".apk");
        universalApk.renameTo(apk);

        // cleanup
        FileUtils.deleteDirectory(apksDir);

        return apk;
    }


    public static File create(File aab, Project project, File bundleDir, ICanceled canceled) throws IOException, CompileExceptionError {
        // STEP 1. Create universal APK set
        File apks = createUniversalApks(aab, bundleDir, canceled);

        // STEP 2. Extract universal.apk from APK set
        File apk = extractUniversalApk(apks, bundleDir);

        // cleanup
        apks.delete();

        return apk;
    }
}
