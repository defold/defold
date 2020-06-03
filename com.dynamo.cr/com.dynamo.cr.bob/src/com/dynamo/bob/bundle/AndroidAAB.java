package com.dynamo.bob.bundle;

import static org.apache.commons.io.FilenameUtils.normalize;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.nio.file.Files;
import java.nio.file.Path;
import java.lang.StringBuilder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.HashSet;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.pipeline.ExtenderUtil;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.Exec;
import com.dynamo.bob.util.Exec.Result;

import com.defold.extender.client.ExtenderResource;

public class AndroidAAB {
	private static Logger logger = Logger.getLogger(AndroidAAB.class.getName());

	private static Hashtable<Platform, String> platformToStripToolMap = new Hashtable<Platform, String>();
	static {
		platformToStripToolMap.put(Platform.Armv7Android, "strip_android");
		platformToStripToolMap.put(Platform.Arm64Android, "strip_android_aarch64");
	}

	private static Hashtable<Platform, String> platformToLibMap = new Hashtable<Platform, String>();
	static {
		platformToLibMap.put(Platform.Armv7Android, "armeabi-v7a");
		platformToLibMap.put(Platform.Arm64Android, "arm64-v8a");
	}

	private static void log(String s) { logger.log(Level.INFO, s); }

	private static void logResourceMap(Map<String, IResource> map) {
		for (String key : map.keySet()) {
			IResource value = map.get(key);
			log("key = " + key + " value = " + value);
		}
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
		File dir = new File(parent, child);
		if (dir.exists()) {
			FileUtils.deleteDirectory(dir);
		}
		dir.mkdirs();
		return dir;
	}

	private static Result exec(List<String> args) throws IOException {
		log("exec: " + String.join(" ", args));
		Map<String, String> env = new HashMap<String, String>();
		if (Platform.getHostPlatform() == Platform.X86_64Linux || Platform.getHostPlatform() == Platform.X86Linux) {
			env.put("LD_LIBRARY_PATH", Bob.getPath(String.format("%s/lib", Platform.getHostPlatform().getPair())));
		}
		return Exec.execResultWithEnvironment(env, args);
	}


	private static String getProjectTitle(Project project) {
		BobProjectProperties projectProperties = project.getProjectProperties();
		String title = projectProperties.getStringValue("project", "title", "Unnamed");
		return title;
	}


	private static String getBinaryNameFromProject(Project project) {
		String title = getProjectTitle(project);
		String exeName = BundleHelper.projectNameToBinaryName(title);
		return exeName;
	}


	private static List<Platform> getArchitectures(Project project) {
		final Platform platform = Platform.Armv7Android;
		return Platform.getArchitecturesFromString(project.option("architectures", ""), platform);
	}


	// baseDir:		/absolute/path/to/base/dir
	// file:		/absolute/path/to/base/dir/some/dir/in/zip/foo.bar
	// result:		some/dir/in/zip/foo.bar
	private static String stripBaseDir(File baseDir, File file) {
		String absoluteBase = baseDir.getAbsolutePath();
		if (!absoluteBase.endsWith(File.separator)) {
			absoluteBase = absoluteBase + File.separator;
		}
		String absoluteFile = file.getAbsolutePath();
		String strippedPath = absoluteFile.replace(absoluteBase, "");
		return strippedPath;
	}

	private static void zipFile(ZipOutputStream zipOut, File baseDir, File file) throws IOException {
		final String filePath = stripBaseDir(baseDir, file);
		final long fileSize = file.length();
		log("Zip " + filePath);

		ZipEntry ze = new ZipEntry(filePath);
		ze.setSize(fileSize);
		byte[] entryData = null;
		CRC32 crc = null;

		// Some files need to be STORED instead of DEFLATED to
		// get "correct" memory mapping at runtime.
		int zipMethod = ZipEntry.DEFLATED;
		boolean isAsset = filePath.startsWith("assets");
		if (isAsset) {
			// Set up an uncompressed file, unfortunately need to calculate crc32 and other data for this to work.
			// https://blogs.oracle.com/CoreJavaTechTips/entry/creating_zip_and_jar_files
			crc = new CRC32();
			zipMethod = ZipEntry.STORED;
			ze.setCompressedSize(fileSize);
		}

		ByteArrayOutputStream byteOut = new ByteArrayOutputStream();
		try {
			if (fileSize > 0) {
				int count;
				entryData = new byte[(int) fileSize];
				InputStream stream = new FileInputStream(file);
				while((count = stream.read(entryData, 0, (int)fileSize)) != -1) {
					byteOut.write(entryData, 0, count);
					if (zipMethod == ZipEntry.STORED) {
						crc.update(entryData, 0, count);
					}
				}
			}
		} finally {
			if(null != byteOut) {
				byteOut.close();
				entryData = byteOut.toByteArray();
			}
		}

		if (zipMethod == ZipEntry.STORED) {
			ze.setCrc(crc.getValue());
			ze.setMethod(zipMethod);
		}

		zipOut.putNextEntry(ze);
		zipOut.write(entryData);
		zipOut.closeEntry();
	}

	private static void zipDir(ZipOutputStream zipOut, File baseDir, File dir, ICanceled canceled) throws IOException {
		for (File f : dir.listFiles()) {
			if (f.isDirectory()) {
				zipDir(zipOut, baseDir, f, canceled);
				BundleHelper.throwIfCanceled(canceled);
			}
			else {
				zipFile(zipOut, baseDir, f);
				BundleHelper.throwIfCanceled(canceled);
			}
		}
	}

	/**
	* Create module zip (for use when creating an .aab)
	*/
	private static void zipModule(File inDir, File outFile, ICanceled canceled) throws IOException {
		if (outFile.exists()) {
			outFile.delete();
		}
		outFile.createNewFile();
		ZipOutputStream zipOut = null;
		try {
			zipOut = new ZipOutputStream(new FileOutputStream(outFile));
			zipDir(zipOut, inDir, inDir, canceled);
		}
		finally {
			IOUtils.closeQuietly(zipOut);
		}
	}

	/**
	* Copy an engine binary to a destination file and optionally strip it of debug symbols
	*/
	private static void copyEngineBinary(Project project, Platform architecture, File dest) throws IOException {
		// vanilla or extender exe?
		final String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");
		List<File> bundleExe = Bob.getNativeExtensionEngineBinaries(architecture, extenderExeDir);
		if (bundleExe == null) {
			final String variant = project.option("variant", Bob.VARIANT_RELEASE);
			bundleExe = Bob.getDefaultDmengineFiles(architecture, variant);
		}

		// copy the exe
		File exe = bundleExe.get(0);
		FileUtils.copyFile(exe, dest);

		// possibly also strip it
		final boolean strip_executable = project.hasOption("strip-executable");
		if (strip_executable) {
			String stripTool = Bob.getExe(Platform.getHostPlatform(), platformToStripToolMap.get(architecture));
			List<String> args = new ArrayList<String>();
			args.add(stripTool);
			args.add(dest.getAbsolutePath());
			Result res = exec(args);
			if (res.ret != 0) {
				throw new IOException(new String(res.stdOutErr));
			}
		}
	}

	/**
	* Get a list of dex files to include in the aab
	*/
	private static ArrayList<File> getClassesDex(Project project) throws IOException {
		String extenderExeDir = FilenameUtils.concat(project.getRootDirectory(), "build");
		ArrayList<File> classesDex = new ArrayList<File>();

		for (Platform architecture : getArchitectures(project)) {
			List<File> bundleExe = Bob.getNativeExtensionEngineBinaries(architecture, extenderExeDir);
			if (bundleExe == null) {
				if (classesDex.isEmpty()) {
					classesDex.add(new File(Bob.getPath("lib/classes.dex")));
				}
			}
			else {
				logger.log(Level.INFO, "Using extender binary for architecture: " + architecture.toString());
				if (classesDex.isEmpty()) {
					int i = 1;
					while(true) {
						String name = i == 1 ? "classes.dex" : String.format("classes%d.dex", i);
						++i;

						File f = new File(FilenameUtils.concat(extenderExeDir, FilenameUtils.concat(architecture.getExtenderPair(), name)));
						if (!f.exists())
						break;
						classesDex.add(f);
					}
				}
			}
		}
		return classesDex;
	}


	private static File copyAppManifestFile(Project project, File bundleDir, File appDir) throws IOException, CompileExceptionError {
		final String variant = project.option("variant", Bob.VARIANT_RELEASE);
		BundleHelper helper = new BundleHelper(project, Platform.Armv7Android, bundleDir, variant);
		return helper.copyOrWriteManifestFile(Platform.Armv7Android, appDir);
	}

	/**
	* Copy Android resources per package
	*/
	private static File copyResources(Project project, File bundleDir, ICanceled canceled) throws IOException {
		// create a list of resource directories, per package
		Platform platform = Platform.Armv7Android;
		File packagesDir = new File(project.getRootDirectory(), "build/"+platform.getExtenderPair()+"/packages");
		List<File> directories = new ArrayList<>();
		File packagesList = new File(packagesDir, "packages.txt");
		if (packagesList.exists()) {
			List<String> allLines = Files.readAllLines(new File(packagesDir, "packages.txt").toPath());
			for (String line : allLines) {
				File resDir = new File(packagesDir, line);
				if (resDir.isDirectory()) {
					directories.add(resDir);
				}
			}
		}
		else {
			for (File dir : packagesDir.listFiles(File::isDirectory)) {
				File resDir = new File(dir, "res");
				if (resDir.isDirectory()) {
					directories.add(resDir);
				}
			}
		}

		// copy all resources per package
		// packages/com.foo.bar/res/* -> res/com.foo.bar/*
		File androidResDir = createDir(bundleDir, "res");
		for (File src : directories) {
			Path srcPath = Path.of(src.getAbsolutePath());
			Path relative = Path.of(packagesDir.getAbsolutePath()).relativize(srcPath);
			String packageName = relative.getName(0).toString();
			File dest = createDir(androidResDir, packageName);
			for(File f : src.listFiles(File::isDirectory)) {
				FileUtils.copyDirectoryToDirectory(f, dest);
				BundleHelper.throwIfCanceled(canceled);
			}
		}
		return androidResDir;
	}

	/**
	* Compile android resources into "flat" files
	* https://developer.android.com/studio/build/building-cmdline#compile_and_link_your_apps_resources
	*/
	private static File aapt2CompileResources(Project project, File bundleDir, File androidResDir, ICanceled canceled) throws CompileExceptionError {
		log("Compiling resources from " + androidResDir.getAbsolutePath());
		try {
			// compile the resources using aapt2 to flat format files
			File aabDir = new File(bundleDir, "aab");
			File compiledResourcesDir = createDir(aabDir, "aapt2/compiled_resources");

			// compile the resources for each package
			for (File packageDir : androidResDir.listFiles(File::isDirectory)) {
				File compiledResourceDir = createDir(compiledResourcesDir, packageDir.getName());

				List<String> args = new ArrayList<String>();
				args.add(Bob.getExe(Platform.getHostPlatform(), "aapt2"));
				args.add("compile");
				args.add("-o"); args.add(compiledResourceDir.getAbsolutePath());
				args.add("--dir"); args.add(packageDir.getAbsolutePath());

				Result res = exec(args);
				if (res.ret != 0) {
					String msg = new String(res.stdOutErr);
					throw new IOException(msg);
				}
				BundleHelper.throwIfCanceled(canceled);
			}

			return compiledResourcesDir;
		} catch (Exception e) {
			throw new CompileExceptionError(null, -1, "Failed compiling Android resources: " + e.getMessage());
		}
	}


	/**
	* Create apk from compiled resources and manifest file.
	* https://developer.android.com/studio/build/building-cmdline#compile_and_link_your_apps_resources
	*/
	private static File aapt2LinkResources(Project project, File bundleDir, File compiledResourcesDir, File manifestFile, ICanceled canceled) throws CompileExceptionError {
		log("Linking resources from " + compiledResourcesDir.getAbsolutePath());
		try {
			File aabDir = new File(bundleDir, "aab");
			File apkDir = createDir(aabDir, "aapt2/apk");
			File outApk = new File(apkDir, "output.apk");

			List<String> args = new ArrayList<String>();
			args.add(Bob.getExe(Platform.getHostPlatform(), "aapt2"));
			args.add("link");
			args.add("--proto-format");
			args.add("-o"); args.add(outApk.getAbsolutePath());
			args.add("-I"); args.add(Bob.getPath("lib/android.jar"));
			args.add("--manifest"); args.add(manifestFile.getAbsolutePath());
			args.add("--auto-add-overlay");

			// write compiled resource list to a txt file
			StringBuilder sb = new StringBuilder();
			for (File resDir : compiledResourcesDir.listFiles(File::isDirectory)) {
				for (File file : resDir.listFiles()) {
					if (file.getAbsolutePath().endsWith(".flat")) {
						sb.append(file.getAbsolutePath() + " ");
					}
				}
			}
			File resourceList = new File(aabDir, "compiled_resources.txt");
			try (BufferedWriter writer = new BufferedWriter(new FileWriter(resourceList))) {
				writer.write(sb.toString());
			}
			args.add("-R"); args.add("@" + resourceList.getAbsolutePath());

			Result res = exec(args);
			if (res.ret != 0) {
				String msg = new String(res.stdOutErr);
				throw new IOException(msg);
			}
			BundleHelper.throwIfCanceled(canceled);
			return outApk;
		} catch (Exception e) {
			throw new CompileExceptionError(null, -1, "Failed building Android Application Bundle: " + e.getMessage());
		}
	}


	/**
	* Package pre-compiled code and resources
	* https://developer.android.com/studio/build/building-cmdline#package_pre-compiled_code_and_resources
	*/
	private static File createAppBundleBaseZip(Project project, File bundleDir, File apk, ICanceled canceled) throws CompileExceptionError {
		log("Creating AAB base.zip");
		try {
			File aabDir = new File(bundleDir, "aab");

			// unzip the generated apk
			File apkUnzipDir = createDir(aabDir, "apk");
			BundleHelper.unzip(new FileInputStream(apk), apkUnzipDir.toPath());

			// create folder structure for the base.zip AAB module
			File baseDir = createDir(aabDir, "base");
			File dexDir = createDir(baseDir, "dex");
			File manifestDir = createDir(baseDir, "manifest");
			File assetsDir = createDir(baseDir, "assets");
			File libDir = createDir(baseDir, "lib");
			File resDir = createDir(baseDir, "res");

			FileUtils.copyFile(new File(apkUnzipDir, "AndroidManifest.xml"), new File(manifestDir, "AndroidManifest.xml"));
			FileUtils.copyFile(new File(apkUnzipDir, "resources.pb"), new File(baseDir, "resources.pb"));

			// copy classes.dex
			ArrayList<File> classesDex = getClassesDex(project);
			for (File classDex : classesDex) {
				log("Copying dex to " + classDex);
				FileUtils.copyFile(classDex, new File(dexDir, classDex.getName()));
				BundleHelper.throwIfCanceled(canceled);
			}

			// copy assets
			Map<String, IResource> bundleResources = ExtenderUtil.collectBundleResources(project, getArchitectures(project));
			ExtenderUtil.writeResourcesToDirectory(bundleResources, assetsDir);
			for (String name : Arrays.asList("game.projectc", "game.arci", "game.arcd", "game.dmanifest", "game.public.der")) {
				File source = new File(new File(project.getRootDirectory(), project.getBuildDirectory()), name);
				File dest = new File(assetsDir, name);
				log("Copying asset to " + dest);
				FileUtils.copyFile(source, dest);
				BundleHelper.throwIfCanceled(canceled);
			}

			// copy resources
			log("Copying resources to " + resDir);
			FileUtils.copyDirectory(new File(apkUnzipDir, "res"), resDir);

			// copy libs
			final String exeName = getBinaryNameFromProject(project);
			for (Platform architecture : getArchitectures(project)) {
				File architectureDir = createDir(libDir, platformToLibMap.get(architecture));
				File dest = new File(architectureDir, "lib" + exeName + ".so");
				log("Copying engine to " + dest);
				copyEngineBinary(project, architecture, dest);
				BundleHelper.throwIfCanceled(canceled);
			}

			// create base.zip
			File baseZip = new File(aabDir, "base.zip");
			log("Zipping " + baseDir + " to " + baseZip);
			zipModule(baseDir, baseZip, canceled);
			return baseZip;
		} catch (Exception e) {
			throw new CompileExceptionError(null, -1, "Failed building Android Application Bundle: " + e.getMessage());
		}
	}

	/**
	* Build the app bundle using bundletool
	* https://developer.android.com/studio/build/building-cmdline#build_your_app_bundle_using_bundletool
	*/
	private static File createBundle(Project project, File bundleDir, File baseZip, ICanceled canceled) throws CompileExceptionError {
		log("Creating Android Application Bundle");
		try {
			File bundletool = new File(Bob.getLibExecPath("bundletool-all.jar"));

			File baseAab = new File(bundleDir, getProjectTitle(project) + ".aab");

			List<String> args = new ArrayList<String>();
			args.add("java"); args.add("-jar");
			args.add(bundletool.getAbsolutePath());
			args.add("build-bundle");
			args.add("--modules"); args.add(baseZip.getAbsolutePath());
			args.add("--output"); args.add(baseAab.getAbsolutePath());

			Result res = exec(args);
			if (res.ret != 0) {
				String msg = new String(res.stdOutErr);
				throw new IOException(msg);
			}
			BundleHelper.throwIfCanceled(canceled);
			return baseAab;
		} catch (Exception e) {
			throw new CompileExceptionError(null, -1, "Failed building Android Application Bundle: " + e.getMessage());
		}
	}

	public static void create(Project project, File bundleDir, ICanceled canceled) throws IOException, CompileExceptionError {
		Bob.initAndroid(); // extract resources

		String title = getProjectTitle(project);
		File appDir = createDir(bundleDir, title);

		BundleHelper.throwIfCanceled(canceled);

		// STEP 1. copy android resources (icons, extension resources and manifest)
		File manifestFile = copyAppManifestFile(project, bundleDir, appDir);
		File androidResDir = copyResources(project, appDir, canceled);

		// STEP 2. Use aapt2 to compile resources (to *.flat files)
		File compiledResDir = aapt2CompileResources(project, appDir, androidResDir, canceled);

		// STEP 3. Use aapt2 to create an APK containing resource files in protobuf format
		File apk = aapt2LinkResources(project, appDir, compiledResDir, manifestFile, canceled);

		// STEP 4. Extract protobuf files from the APK and create base.zip (manifest, assets, dex, res, lib, *.pb etc)
		File baseZip = createAppBundleBaseZip(project, appDir, apk, canceled);

		// STEP 5. Use bundletool to create AAB from base.zip
		// TODO: INVESTIGATE: Some files need to be STORED instead of DEFLATED to get "correct" memory mapping at runtime
		File baseAab = createBundle(project, appDir, baseZip, canceled);
	}
}
