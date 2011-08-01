package com.dynamo.cr.editor.core.internal.refactoring;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.OperationCanceledException;
import org.eclipse.core.runtime.Path;
import org.eclipse.ltk.core.refactoring.Change;
import org.eclipse.ltk.core.refactoring.RefactoringStatus;
import org.eclipse.ltk.core.refactoring.participants.CheckConditionsContext;
import org.eclipse.ltk.core.refactoring.participants.RenameParticipant;

import com.dynamo.cr.editor.core.internal.refactoring.ParticipantUtils.Element;

public class ResourceRenameParticipant extends RenameParticipant {

    private List<Element> elements = new ArrayList<Element>();

    public ResourceRenameParticipant() {
    }

    @Override
    protected boolean initialize(Object element) {
        if (element instanceof IFile) {
            IFile fromFile = (IFile) element;
            IFile toFile = fromFile.getParent().getFile(new Path(getArguments().getNewName()));
            elements.add(new Element(fromFile, toFile));
            return true;
        }
        else if (element instanceof IContainer) {
            final IFolder folder = (IFolder) element;
            final IFolder newFolder = folder.getParent().getFolder(new Path(getArguments().getNewName()));
            try {
                folder.accept(new IResourceVisitor() {

                    @Override
                    public boolean visit(IResource resource) throws CoreException {
                        if (resource instanceof IFile) {
                            IFile file = (IFile) resource;
                            // Only add files with extension
                            if (file.getFileExtension() != null) {
                                IPath pathTo = file.getFullPath().makeRelativeTo(folder.getFullPath());
                                IFile toFile = newFolder.getFile(pathTo);
                                elements.add(new Element(file, toFile));
                            }
                        }
                        return true;
                    }
                });
            } catch (CoreException e) {
                // Can't use any UI here in a core plugin such as StatusManager
                e.printStackTrace();
            }
            return true;
        }
        return false;
    }

    @Override
    public String getName() {
        return "Update references in resources";
    }

    @Override
    public RefactoringStatus checkConditions(IProgressMonitor pm,
            CheckConditionsContext context) throws OperationCanceledException {
        return new RefactoringStatus();
    }

    @Override
    public Change createChange(IProgressMonitor pm) throws CoreException,
            OperationCanceledException {
        return null;
    }

    @Override
    public Change createPreChange(IProgressMonitor pm) throws CoreException,
            OperationCanceledException {
        if (getArguments().getUpdateReferences())
            return ParticipantUtils.moveFiles(pm, elements);
        else
            return null;
    }

}
