package com.dynamo.cr.editor.core.internal.refactoring;

import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.OperationCanceledException;
import org.eclipse.ltk.core.refactoring.Change;
import org.eclipse.ltk.core.refactoring.RefactoringStatus;
import org.eclipse.ltk.core.refactoring.participants.CheckConditionsContext;
import org.eclipse.ltk.core.refactoring.participants.DeleteParticipant;
import org.eclipse.ltk.core.refactoring.participants.ISharableParticipant;
import org.eclipse.ltk.core.refactoring.participants.RefactoringArguments;

public class ResourceDeleteParticipant extends DeleteParticipant implements ISharableParticipant{

    private List<IFile> elements = new ArrayList<IFile>();

    public ResourceDeleteParticipant() {
    }

    @Override
    protected boolean initialize(Object element) {
        addElement(element, getArguments());
        return elements.size() > 0;
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

        return ParticipantUtils.deleteFiles(pm, elements);
    }

    @Override
    public void addElement(Object element, RefactoringArguments arguments) {
        if (element instanceof IFile) {
            IFile file = (IFile) element;
            elements.add(file);
        }
        else if (element instanceof IContainer) {
            final IFolder folder = (IFolder) element;
            try {
                folder.accept(new IResourceVisitor() {

                    @Override
                    public boolean visit(IResource resource) throws CoreException {
                        if (resource instanceof IFile) {
                            IFile file = (IFile) resource;
                            // Only add files with extension
                            if (file.getFileExtension() != null) {
                                elements.add(file);
                            }
                        }
                        return true;
                    }
                });
            } catch (CoreException e) {
                e.printStackTrace();
            }
        }
    }
}
