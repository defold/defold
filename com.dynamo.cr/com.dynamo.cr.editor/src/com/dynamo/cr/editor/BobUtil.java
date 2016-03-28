package com.dynamo.cr.editor;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import org.apache.commons.codec.binary.Base64;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.lang.SerializationUtils;
import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileInfo;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.bob.LibraryException;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.util.LibraryUtil;
import com.dynamo.cr.editor.builders.ProgressDelegate;
import com.dynamo.cr.editor.core.ProjectProperties;

public class BobUtil {

    /**
     * Retrieve bob specific args from the supplied args map.
     * Bob args is a serialized HashMap stored with key bobArgs.
     */
    @SuppressWarnings("unchecked")
    public static final Map<String, String> getBobArgs(Map<String, String> args) {
        String bobArgsEncoded = args.get("bobArgs");
        if (bobArgsEncoded != null) {
            return (Map<String, String>) SerializationUtils.deserialize(Base64.decodeBase64(bobArgsEncoded));
        }
        return null;
    }

    /**
     * Insert bob specific args into the supplied args map.
     * Bob args is a serialized HashMap stored with key bobArgs.
     */
    public static final void putBobArgs(HashMap<String, String> bobArgs, Map<String, String> dstArgs) {
        String bobArgsEncoded = Base64.encodeBase64String(SerializationUtils.serialize(bobArgs));
        dstArgs.put("bobArgs", bobArgsEncoded);
    }

    @SuppressWarnings("unchecked")
    public static final ArrayList<String> getBobCommands(Map<String, String> commands) {
        String bobCommandsEncoded = commands.get("bobCommands");
        if (bobCommandsEncoded != null) {
            return (ArrayList<String>) SerializationUtils.deserialize(Base64.decodeBase64(bobCommandsEncoded));
        }
        return null;
    }

    public static final void putBobCommands(ArrayList<String> commands, Map<String, String> dstArgs) {
        String bobCommandsEncoded = Base64.encodeBase64String(SerializationUtils.serialize(commands));
        dstArgs.put("bobCommands", bobCommandsEncoded);
    }

    public static List<URL> getLibraryUrls(String rootPath) throws LibraryException {
        List<URL> urls = new ArrayList<URL>();
        ProjectProperties properties = new ProjectProperties();
        File projectProps = new File(FilenameUtils.concat(rootPath, "game.project"));
        InputStream input = null;
        try {
            input = new BufferedInputStream(new FileInputStream(projectProps));
            properties.load(input);
        } catch (Exception e) {
            throw new LibraryException("Failed to parse game.project", e);
        } finally {
            IOUtils.closeQuietly(input);
        }
        String dependencies = properties.getStringValue("project", "dependencies", null);
        if (dependencies != null) {
            urls = LibraryUtil.parseLibraryUrls(dependencies);
        }
        return urls;
    }

    private static CoreException wrapCoreException(Exception e) {
        return new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e));
    }

    private static void linkLibraries(IFolder contentRoot, List<String> libPaths, IProgressMonitor monitor) throws CoreException {
        IResource[] members = contentRoot.members();
        for (IResource member : members) {
            if (member instanceof IFolder) {
                IFolder folder = (IFolder)member;
                URI uri = folder.getLocationURI();
                if (folder.isLinked() && uri.getScheme() != null && uri.getScheme().equals("zip")) {
                    IFileStore store = EFS.getStore(uri);
                    boolean exists = false;
                    try {
                        IFileInfo info = store.fetchInfo(EFS.NONE, monitor);
                        exists = info.exists();
                    } catch (CoreException e) {
                    }
                    if (!exists) {
                        folder.delete(true, monitor);
                    }
                }
            }
        }
        for (String libPath : libPaths) {
            IResource lib = contentRoot.findMember(libPath);
            if (lib != null && lib.exists()) {
                try {
                    Set<String> roots = new HashSet<String>();
                    ZipFile zip = new ZipFile(lib.getLocation().toFile());
                    try {
                        String includeBaseDir = LibraryUtil.findIncludeBaseDir(zip);
                        Set<String> includeRoots = LibraryUtil.readIncludeDirsFromArchive(includeBaseDir, zip);
                        Enumeration<? extends ZipEntry> entries = zip.entries();
                        while (entries.hasMoreElements()) {
                            ZipEntry entry = entries.nextElement();
                            Path path = new Path(entry.getName().substring(includeBaseDir.length()));
                            if ((entry.isDirectory() && path.segmentCount() == 1) || path.segmentCount() > 1) {
                                String dir = path.segment(0);
                                if (!dir.isEmpty() && includeRoots.contains(dir)) {
                                    roots.add(dir);
                                }
                            }
                        }
                        for (String root : roots) {
                            URI libUri = new URI("zip", null, "/" + includeBaseDir + root, lib.getLocationURI().toString(), null);
                            IFolder rootFolder = contentRoot.getFolder(root);
                            rootFolder.createLink(libUri, IResource.REPLACE | IResource.ALLOW_MISSING_LOCAL, monitor);
                        }
                    } finally {
                        zip.close();
                    }
                } catch (URISyntaxException e) {
                    throw wrapCoreException(e);
                } catch (IOException e) {
                    throw wrapCoreException(e);
                } catch (ParseException e) {
                    throw wrapCoreException(e);
                }
            }
        }
    }

    public static void resolveLibs(IFolder contentRoot, String email, String auth, IProgressMonitor monitor) throws CoreException {
        IFolder libFolder = contentRoot.getFolder(Project.LIB_DIR);
        try {
            List<URL> libUrls = BobUtil.getLibraryUrls(contentRoot.getLocation().toOSString());
            Project project = new Project(new DefaultFileSystem(), contentRoot.getLocation().toOSString(), "build/default");
            project.setOption("email", email);
            project.setOption("auth", auth);
            project.setLibUrls(libUrls);
            project.resolveLibUrls(new ProgressDelegate(monitor));
            libFolder.refreshLocal(1, monitor);
            List<String> libPaths = new ArrayList<String>(libUrls.size());
            IPath libPath = libFolder.getProjectRelativePath().makeRelativeTo(contentRoot.getProjectRelativePath());
            for (URL url : libUrls) {
                String fileName = LibraryUtil.libUrlToFilename(url);
                libPaths.add(libPath.append(fileName).toOSString());
            }
            linkLibraries(contentRoot, libPaths, monitor);
            contentRoot.refreshLocal(1, monitor);
        } catch (IOException e) {
            throw wrapCoreException(e);
        } catch (LibraryException e) {
            throw wrapCoreException(e);
        }
    }
}
