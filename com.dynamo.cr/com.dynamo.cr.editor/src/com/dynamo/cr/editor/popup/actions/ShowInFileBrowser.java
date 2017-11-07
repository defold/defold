package com.dynamo.cr.editor.popup.actions;

import java.io.File;
import java.net.URI;

import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.action.IAction;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.ui.IActionDelegate;
import org.eclipse.ui.IObjectActionDelegate;
import org.eclipse.ui.IWorkbenchPart;

import com.dynamo.cr.editor.Activator;
import com.dynamo.cr.editor.core.EditorCorePlugin;

public class ShowInFileBrowser implements IObjectActionDelegate {

    private String folder;

    /**
     * Constructor for Action1.
     */
    public ShowInFileBrowser() {
        super();
    }

    /**
     * @see IObjectActionDelegate#setActivePart(IAction, IWorkbenchPart)
     */
    public void setActivePart(IAction action, IWorkbenchPart targetPart) {
    }

    /**
     * @see IActionDelegate#run(IAction)
     */
    public void run(IAction action) {

        String plat = EditorCorePlugin.getPlatform();
        String cmd = null;
        if (plat.contains("darwin")) {
            cmd = String.format("open %s", this.folder);
        } else if (plat.contains("win32")) {
            cmd = String.format("explorer %s", this.folder);
        } else if (plat.contains("linux")) {
            cmd = String.format("gnome-open %s", this.folder);
        }

        Process proc;
        try {
            proc = Runtime.getRuntime().exec(cmd);
            proc.waitFor();
        } catch (Exception e) {
            Activator.logException(e);
        }
    }

    /**
     * @see IActionDelegate#selectionChanged(IAction, ISelection)
     */
    public void selectionChanged(IAction action, ISelection selection) {
        action.setEnabled(false);

        if (!selection.isEmpty()) {
            IResource r = (IResource) ((IStructuredSelection) selection)
                    .getFirstElement();
            if (r instanceof IFile || r instanceof IFolder) {
                URI uri = r.getLocationURI();
                try {
                    IFileStore store = EFS.getStore(uri);

                    File localFile = store.toLocalFile(0,
                            new NullProgressMonitor());
                    if (localFile != null) {

                        IPath path = new Path(localFile.getAbsolutePath());
                        if (localFile.isFile()) {
                            path = path.removeLastSegments(1);
                        }
                        this.folder = path.toOSString();
                        action.setEnabled(true);
                    }
                } catch (CoreException e) {
                    Activator.logException(e);
                }
            }
        }
    }
}
