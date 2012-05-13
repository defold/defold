package com.dynamo.cr.editor.core;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.Path;

public class EditorUtil {

    /**
     * Get the game.project file by searching downwards in the file system.
     * @param container the container to start searching
     * @return game.project file. null if the directory structure does not contain it
     */
    public static IFile findGameProjectFile(IContainer container) {
        IContainer c = container;
        while (c != null) {
            IFile f = c.getFile(new Path("game.project"));
            if (f.exists()) {
                return f;
            }
            c = c.getParent();
        }
        return null;
    }

    /**
     * Get the content-root, ie the container that contains the file game.project given a file within the content directory structure.
     * @param file the file to the content root for
     * @return content-root container. null of game.project doesn't exists within the directory structure.
     */
    public static IContainer findContentRoot(IResource file) {
        IContainer c = file.getParent();
        IFile f = findGameProjectFile(c);
        if (f != null) {
            return f.getParent();
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
     * Get current cr project or actually the first project with nature
     * "com.dynamo.cr.editor.core.crnature" set. We assume a single project.
     * @return project. null if project can be found.
     */
    public static IProject getProject() {
        try {
            IProject[] projects = ResourcesPlugin.getWorkspace().getRoot().getProjects();
            for (IProject project : projects) {
                if (project.isOpen() && project.hasNature("com.dynamo.cr.editor.core.crnature")) {
                    return project;
                }
            }
        } catch (CoreException e) {
            EditorCorePlugin.logException(e);
            return null;
        }
        return null;
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
