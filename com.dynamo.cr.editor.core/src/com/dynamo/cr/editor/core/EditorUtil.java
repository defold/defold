package com.dynamo.cr.editor.core;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;

public class EditorUtil {

    /**
     * Get the content-root, ie the container that contains the file game.project given a file within the content directory structure.
     * @param file the file to the content root for
     * @return content-root container. null of game.project doesn't exists within the directory structure.
     */
    public static IContainer findContentRoot(IFile file) {
        IContainer c = file.getParent();
        while (c != null) {
            if (c instanceof IFolder) {
                IFolder folder = (IFolder) c;
                IFile f = folder.getFile("game.project");
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
}
