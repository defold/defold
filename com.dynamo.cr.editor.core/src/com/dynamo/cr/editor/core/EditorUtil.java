package com.dynamo.cr.editor.core;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.Path;

public class EditorUtil {

    /**
     * Get the content-root, ie the container that contains the file game.project given a file within the content directory structure.
     * @param file the file to the content root for
     * @return content-root container. null of game.project doesn't exists within the directory structure.
     */
    public static IContainer findContentRoot(IResource file) {
        IContainer c = file.getParent();
        while (c != null) {
            if (c instanceof IContainer) {
                IContainer folder = (IContainer) c;
                IFile f = folder.getFile(new Path("game.project"));
                if (f.exists()) {
                    return c;
                }
            }
            c = c.getParent();
        }
        return null;
    }

    /**
     * Get content root given a project
     * @param project project to get content root for
     * @return content root
     */
    public static IFolder getContentRoot(IProject project) {
        // NOTE: In the "future" we might change "content". Thats why we have this method.
        return project.getFolder("content");
    }

    /**
     * Create an absolute resource path, from content root, to a resource
     * @param resource resource to create resource path for
     * @return Absolute path to file
     */
    public static String makeResourcePath(IResource resource) {
        IContainer contentRoot = findContentRoot(resource);
        if (contentRoot == null) {
            throw new RuntimeException(String.format("Content root for file '%s' not found", resource.getFullPath().toPortableString()));
        }
        String path = "/" + resource.getFullPath().makeRelativeTo(contentRoot.getFullPath()).toPortableString();
        return path;
    }
}
